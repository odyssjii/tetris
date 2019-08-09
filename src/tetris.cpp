#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <SDL.h>
#include <SDL_ttf.h>


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef float f32;
typedef double f64;

#include "colors.h"

#define WIDTH 10
#define HEIGHT 22
#define VISIBLE_HEIGHT 20
#define GRID_SIZE 30

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

const u8 FRAMES_PER_DROP[] = {
    48,
    43,
    38,
    33,
    28,
    23,
    18,
    13,
    8,
    6,
    5,
    5,
    5,
    4,
    4,
    4,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1
};

const f32 TARGET_SECONDS_PER_FRAME = 1.f / 60.f;

struct Tetrino
{
    const u8 *data;
    const s32 side;
};

inline Tetrino
tetrino(const u8 *data, s32 side)
{
    return { data, side };
}

const u8 TETRINO_1[] = {
    0, 0, 0, 0,
    1, 1, 1, 1,
    0, 0, 0, 0,
    0, 0, 0, 0
};

const u8 TETRINO_2[] = {
    2, 2,
    2, 2
};

const u8 TETRINO_3[] = {
    0, 0, 0,
    3, 3, 3,
    0, 3, 0
};

const u8 TETRINO_4[] = {
    0, 4, 4,
    4, 4, 0,
    0, 0, 0
};

const u8 TETRINO_5[] = {
    5, 5, 0,
    0, 5, 5,
    0, 0, 0
};

const u8 TETRINO_6[] = {
    6, 0, 0,
    6, 6, 6,
    0, 0, 0
};

const u8 TETRINO_7[] = {
    0, 0, 7,
    7, 7, 7,
    0, 0, 0
};


const Tetrino TETRINOS[] = {
    tetrino(TETRINO_1, 4),
    tetrino(TETRINO_2, 2),
    tetrino(TETRINO_3, 3),
    tetrino(TETRINO_4, 3),
    tetrino(TETRINO_5, 3),
    tetrino(TETRINO_6, 3),
    tetrino(TETRINO_7, 3),
};

enum Game_Phase
{
    GAME_PHASE_START,
    GAME_PHASE_PLAY,
    GAME_PHASE_LINE,
    GAME_PHASE_GAMEOVER
};

struct Piece_State
{
    u8 tetrino_index;
    s32 offset_row;
    s32 offset_col;
    s32 rotation;
};

struct Game_State
{
    u8 board[WIDTH * HEIGHT];
    u8 lines[HEIGHT];
    s32 pending_line_count;
    
    Piece_State piece;

    Game_Phase phase;
    
    s32 start_level;
    s32 level;
    s32 line_count;
    s32 points;
    
    f32 next_drop_time;
    f32 highlight_end_time;
    f32 time;
};

struct Input_State
{
    u8 left;
    u8 right;
    u8 up;
    u8 down;

    u8 a;
    
    s8 dleft;
    s8 dright;
    s8 dup;
    s8 ddown;
    s8 da;
};

enum Text_Align
{
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
};

inline u8
matrix_get(const u8 *values, s32 width, s32 row, s32 col)
{
    s32 index = row * width + col;
    return values[index];
}

inline void
matrix_set(u8 *values, s32 width, s32 row, s32 col, u8 value)
{
    s32 index = row * width + col;
    values[index] = value;
}

inline u8
tetrino_get(const Tetrino *tetrino, s32 row, s32 col, s32 rotation)
{
    s32 side = tetrino->side;
    switch (rotation)
    {
    case 0:
        return tetrino->data[row * side + col];
    case 1:
        return tetrino->data[(side - col - 1) * side + row];
    case 2:
        return tetrino->data[(side - row - 1) * side + (side - col - 1)];
    case 3:
        return tetrino->data[col * side + (side - row - 1)];
    }
    return 0;
}

inline u8
check_row_filled(const u8 *values, s32 width, s32 row)
{
    for (s32 col = 0;
         col < width;
         ++col)
    {
        if (!matrix_get(values, width, row, col))
        {
            return 0;
        }
    }
    return 1;
}

inline u8
check_row_empty(const u8 *values, s32 width, s32 row)
{
    for (s32 col = 0;
         col < width;
         ++col)
    {
        if (matrix_get(values, width, row, col))
        {
            return 0;
        }
    }
    return 1;
}

s32
find_lines(const u8 *values, s32 width, s32 height, u8 *lines_out)
{
    s32 count = 0;
    for (s32 row = 0;
         row < height;
         ++row)
    {
        u8 filled = check_row_filled(values, width, row);
        lines_out[row] = filled;
        count += filled;
    }
    return count;
}

void
clear_lines(u8 *values, s32 width, s32 height, const u8 *lines)
{
    s32 src_row = height - 1;
    for (s32 dst_row = height - 1;
         dst_row >= 0;
         --dst_row)
    {
        while (src_row >= 0 && lines[src_row])
        {
            --src_row;
        }

        if (src_row < 0)
        {
            memset(values + dst_row * width, 0, width);
        }
        else 
        {
            if (src_row != dst_row)
            {
                memcpy(values + dst_row * width,
                       values + src_row * width,
                       width);
            }
            --src_row;
        }
    }
}


bool
check_piece_valid(const Piece_State *piece,
                  const u8 *board, s32 width, s32 height)
{
    const Tetrino *tetrino = TETRINOS + piece->tetrino_index;
    assert(tetrino);

    for (s32 row = 0;
         row < tetrino->side;
         ++row)
    {
        for (s32 col = 0;
             col < tetrino->side;
             ++col)
        {
            u8 value = tetrino_get(tetrino, row, col, piece->rotation);
            if (value > 0)
            {
                s32 board_row = piece->offset_row + row;
                s32 board_col = piece->offset_col + col;
                if (board_row < 0)
                {
                    return false;
                }
                if (board_row >= height)
                {
                    return false;
                }
                if (board_col < 0)
                {
                    return false;
                }
                if (board_col >= width)
                {
                    return false;
                }
                if (matrix_get(board, width, board_row, board_col))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

void
merge_piece(Game_State *game)
{
    const Tetrino *tetrino = TETRINOS + game->piece.tetrino_index;
    for (s32 row = 0;
         row < tetrino->side;
         ++row)
    {
        for (s32 col = 0;
             col < tetrino->side;
             ++col)
        {
            u8 value = tetrino_get(tetrino, row, col, game->piece.rotation);
            if (value)
            {
                s32 board_row = game->piece.offset_row + row;
                s32 board_col = game->piece.offset_col + col;
                matrix_set(game->board, WIDTH, board_row, board_col, value);
            }
        }
    }
}

inline s32
random_int(s32 min, s32 max)
{
    s32 range = max - min;
    return min + rand() % range;
}

inline f32
get_time_to_next_drop(s32 level)
{
    if (level > 29)
    {
        level = 29;
    }
    return FRAMES_PER_DROP[level] * TARGET_SECONDS_PER_FRAME;
}


void
spawn_piece(Game_State *game)
{
    game->piece = {};
    game->piece.tetrino_index = (u8)random_int(0, ARRAY_COUNT(TETRINOS));
    game->piece.offset_col = WIDTH / 2;
    game->next_drop_time = game->time + get_time_to_next_drop(game->level);
}


inline bool
soft_drop(Game_State *game)
{
    ++game->piece.offset_row;
    if (!check_piece_valid(&game->piece, game->board, WIDTH, HEIGHT))
    {
        --game->piece.offset_row;
        merge_piece(game);
        spawn_piece(game);
        return false;
    }

    game->next_drop_time = game->time + get_time_to_next_drop(game->level);
    return true;
}

inline s32
compute_points(s32 level, s32 line_count)
{
    switch (line_count)
    {
    case 1:
        return 40 * (level + 1);
    case 2:
        return 100 * (level + 1);
    case 3:
        return 300 * (level + 1);
    case 4:
        return 1200 * (level + 1);
    }
    return 0;
}

inline s32
min(s32 x, s32 y)
{
    return x < y ? x : y;
}
inline s32
max(s32 x, s32 y)
{
    return x > y ? x : y;
}

inline s32
get_lines_for_next_level(s32 start_level, s32 level)
{
    s32 first_level_up_limit = min(
        (start_level * 10 + 10),
        max(100, (start_level * 10 - 50)));
    if (level == start_level)
    {
        return first_level_up_limit;
    }
    s32 diff = level - start_level;
    return first_level_up_limit + diff * 10;
}

void
update_game_start(Game_State *game, const Input_State *input)
{
    if (input->dup > 0)
    {
        ++game->start_level;
    }

    if (input->ddown > 0 && game->start_level > 0)
    {
        --game->start_level;
    }
    
    if (input->da > 0)
    {
        memset(game->board, 0, WIDTH * HEIGHT);
        game->level = game->start_level;
        game->line_count = 0;
        game->points = 0;
        spawn_piece(game);
        game->phase = GAME_PHASE_PLAY;
    }
}

void
update_game_gameover(Game_State *game, const Input_State *input)
{
    if (input->da > 0)
    {
        game->phase = GAME_PHASE_START;
    }
}

void
update_game_line(Game_State *game)
{
    if (game->time >= game->highlight_end_time)
    {
        clear_lines(game->board, WIDTH, HEIGHT, game->lines);
        game->line_count += game->pending_line_count;
        game->points += compute_points(game->level, game->pending_line_count);

        s32 lines_for_next_level = get_lines_for_next_level(game->start_level,
                                                            game->level);
        if (game->line_count >= lines_for_next_level)
        {
            ++game->level;
        }
        
        game->phase = GAME_PHASE_PLAY;
    }
}

void
update_game_play(Game_State *game,
                 const Input_State *input)
{
    Piece_State piece = game->piece;
    if (input->dleft > 0)
    {
        --piece.offset_col;
    }
    if (input->dright> 0)
    {
        ++piece.offset_col;
    }
    if (input->dup > 0)
    {
        piece.rotation = (piece.rotation + 1) % 4;
    }

    if (check_piece_valid(&piece, game->board, WIDTH, HEIGHT))
    {
        game->piece = piece;
    }

    if (input->ddown > 0)
    {
        soft_drop(game);
    }

    if (input->da > 0)
    {
        while(soft_drop(game));
    }
    
    while (game->time >= game->next_drop_time)
    {
        soft_drop(game);
    }

    game->pending_line_count = find_lines(game->board, WIDTH, HEIGHT, game->lines);
    if (game->pending_line_count > 0)
    {
        game->phase = GAME_PHASE_LINE;
        game->highlight_end_time = game->time + 0.5f;
    }

    s32 game_over_row = 0;
    if (!check_row_empty(game->board, WIDTH, game_over_row))
    {
        game->phase = GAME_PHASE_GAMEOVER;
    }
}

void
update_game(Game_State *game,
            const Input_State *input)
{
    switch(game->phase)
    {
    case GAME_PHASE_START:
        update_game_start(game, input);
        break;
    case GAME_PHASE_PLAY:
        update_game_play(game, input);
        break;
    case GAME_PHASE_LINE:
        update_game_line(game);
        break;
    case GAME_PHASE_GAMEOVER:
        update_game_gameover(game, input);
        break;
    }
}

void
fill_rect(SDL_Renderer *renderer,
          s32 x, s32 y, s32 width, s32 height, Color color)
{
    SDL_Rect rect = {};
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}


void
draw_rect(SDL_Renderer *renderer,
          s32 x, s32 y, s32 width, s32 height, Color color)
{
    SDL_Rect rect = {};
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &rect);
}

void
draw_string(SDL_Renderer *renderer,
            TTF_Font *font,
            const char *text,
            s32 x, s32 y,
            Text_Align alignment,
            Color color)
{
    SDL_Color sdl_color = SDL_Color { color.r, color.g, color.b, color.a };
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, sdl_color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect rect;
    rect.y = y;
    rect.w = surface->w;
    rect.h = surface->h;
    switch (alignment)
    {
    case TEXT_ALIGN_LEFT:
        rect.x = x;
        break;
    case TEXT_ALIGN_CENTER:
        rect.x = x - surface->w / 2;
        break;
    case TEXT_ALIGN_RIGHT:
        rect.x = x - surface->w;
        break;
    }

    SDL_RenderCopy(renderer, texture, 0, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void
draw_cell(SDL_Renderer *renderer,
          s32 row, s32 col, u8 value,
          s32 offset_x, s32 offset_y,
          bool outline = false)
{
    Color base_color = BASE_COLORS[value];
    Color light_color = LIGHT_COLORS[value];
    Color dark_color = DARK_COLORS[value];

   
    s32 edge = GRID_SIZE / 8;

    s32 x = col * GRID_SIZE + offset_x;
    s32 y = row * GRID_SIZE + offset_y;

    if (outline)
    {
        draw_rect(renderer, x, y, GRID_SIZE, GRID_SIZE, base_color);
        return;
    }
    
    fill_rect(renderer, x, y, GRID_SIZE, GRID_SIZE, dark_color);
    fill_rect(renderer, x + edge, y,
              GRID_SIZE - edge, GRID_SIZE - edge, light_color);
    fill_rect(renderer, x + edge, y + edge,
              GRID_SIZE - edge * 2, GRID_SIZE - edge * 2, base_color); 
}

void
draw_piece(SDL_Renderer *renderer,
           const Piece_State *piece,
           s32 offset_x, s32 offset_y,
           bool outline = false)
{
    const Tetrino *tetrino = TETRINOS + piece->tetrino_index;
    for (s32 row = 0;
         row < tetrino->side;
         ++row)
    {
        for (s32 col = 0;
             col < tetrino->side;
             ++col)
        {
            u8 value = tetrino_get(tetrino, row, col, piece->rotation);
            if (value)
            {
                draw_cell(renderer,
                          row + piece->offset_row,
                          col + piece->offset_col,
                          value,
                          offset_x, offset_y,
                          outline);
            }
        }
    }
}

void
draw_board(SDL_Renderer *renderer,
           const u8 *board, s32 width, s32 height,
           s32 offset_x, s32 offset_y)
{
    fill_rect(renderer, offset_x, offset_y,
              width * GRID_SIZE, height * GRID_SIZE,
              BASE_COLORS[0]);
    for (s32 row = 0;
         row < height;
         ++row)
    {
        for (s32 col = 0;
             col < width;
             ++col)
        {
            u8 value = matrix_get(board, width, row, col);
            if (value)
            {
                draw_cell(renderer, row, col, value, offset_x, offset_y);
            }
        }
    }
}

void
render_game(const Game_State *game,
            SDL_Renderer *renderer,
            TTF_Font *font)
{

    char buffer[4096];
    
    Color highlight_color = color(0xFF, 0xFF, 0xFF, 0xFF);

    s32 margin_y = 60;
    
    draw_board(renderer, game->board, WIDTH, HEIGHT, 0, margin_y);

    if (game->phase == GAME_PHASE_PLAY)
    {
        draw_piece(renderer, &game->piece, 0, margin_y);

        Piece_State piece = game->piece;
        while (check_piece_valid(&piece, game->board, WIDTH, HEIGHT))
        {
            piece.offset_row++;
        }
        --piece.offset_row;

        draw_piece(renderer, &piece, 0, margin_y, true);
        
    }

    if (game->phase == GAME_PHASE_LINE)
    {
        for (s32 row = 0;
             row < HEIGHT;
             ++row)
        {
            if (game->lines[row])
            {
                s32 x = 0;
                s32 y = row * GRID_SIZE + margin_y;
                
                fill_rect(renderer, x, y,
                          WIDTH * GRID_SIZE, GRID_SIZE, highlight_color);
            }
        }
    }
    else if (game->phase == GAME_PHASE_GAMEOVER)
    {
        s32 x = WIDTH * GRID_SIZE / 2;
        s32 y = (HEIGHT * GRID_SIZE + margin_y) / 2;
        draw_string(renderer, font, "GAME OVER",
                    x, y, TEXT_ALIGN_CENTER, highlight_color);
    }
    else if (game->phase == GAME_PHASE_START)
    {
        s32 x = WIDTH * GRID_SIZE / 2;
        s32 y = (HEIGHT * GRID_SIZE + margin_y) / 2;
        draw_string(renderer, font, "PRESS START",
                    x, y, TEXT_ALIGN_CENTER, highlight_color);

        snprintf(buffer, sizeof(buffer), "STARTING LEVEL: %d", game->start_level);
        draw_string(renderer, font, buffer,
                    x, y + 30, TEXT_ALIGN_CENTER, highlight_color);
    }
    
    fill_rect(renderer,
              0, margin_y,
              WIDTH * GRID_SIZE, (HEIGHT - VISIBLE_HEIGHT) * GRID_SIZE,
              color(0x00, 0x00, 0x00, 0x00));
    

    snprintf(buffer, sizeof(buffer), "LEVEL: %d", game->level);
    draw_string(renderer, font, buffer, 5, 5, TEXT_ALIGN_LEFT, highlight_color);

    snprintf(buffer, sizeof(buffer), "LINES: %d", game->line_count);
    draw_string(renderer, font, buffer, 5, 35, TEXT_ALIGN_LEFT, highlight_color);
    
    snprintf(buffer, sizeof(buffer), "POINTS: %d", game->points);
    draw_string(renderer, font, buffer, 5, 65, TEXT_ALIGN_LEFT, highlight_color);
}

int
main()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        return 1;
    }

    if (TTF_Init() < 0)
    {
        return 2;
    }
    
    SDL_Window *window = SDL_CreateWindow(
        "Tetris",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        300,
        720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    const char *font_name = "novem___.ttf";
    TTF_Font *font = TTF_OpenFont(font_name, 24);
    
    Game_State game = {};
    Input_State input = {};

    spawn_piece(&game);
    
    game.piece.tetrino_index = 2;
    
    bool quit = false;
    while (!quit)
    {
        game.time = SDL_GetTicks() / 1000.0f;
        
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
        }

        s32 key_count;
        const u8 *key_states = SDL_GetKeyboardState(&key_count);

        if (key_states[SDL_SCANCODE_ESCAPE])
        {
            quit = true;
        }
        
        Input_State prev_input = input;
        
        input.left = key_states[SDL_SCANCODE_LEFT];
        input.right = key_states[SDL_SCANCODE_RIGHT];
        input.up = key_states[SDL_SCANCODE_UP];
        input.down = key_states[SDL_SCANCODE_DOWN];
        input.a = key_states[SDL_SCANCODE_SPACE];

        input.dleft = input.left - prev_input.left;
        input.dright = input.right - prev_input.right;
        input.dup = input.up - prev_input.up;
        input.ddown = input.down - prev_input.down;
        input.da = input.a - prev_input.a;
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        
        update_game(&game, &input);
        render_game(&game, renderer, font);

        SDL_RenderPresent(renderer);
    }
        
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();

    return 0;
}

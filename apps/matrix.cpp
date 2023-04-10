#include <ncurses.h>

#include <algorithm>
#include <deque>
#include <map>
#include <random>
#include <tuple>
#include <vector>

/*


_|      _|    _|_|    _|_|_|_|_|  _|_|_|    _|_|_|  _|      _|
_|_|  _|_|  _|    _|      _|      _|    _|    _|      _|  _|
_|  _|  _|  _|_|_|_|      _|      _|_|_|      _|        _|
_|      _|  _|    _|      _|      _|    _|    _|      _|  _|
_|      _|  _|    _|      _|      _|    _|  _|_|_|  _|      _|


*/

struct Rain
{
    Rain(
            WINDOW* window,
            double density,
            double roll_rate,
            double fade_rate,
            const std::vector<int>& character_set
        ) :
        m_window{window},
        m_density{density},
        m_roll_rate{roll_rate},
        m_fade_rate{fade_rate},
        m_generator{std::random_device{}()},
        m_distribution{0.0, 1.0},
        m_character_set{character_set},
        m_begin_gradient_color_pair{1},
        m_gradient_steps{20},
        m_highlight_color_pair{m_begin_gradient_color_pair + m_gradient_steps},
        m_original_color_indices{},
        m_original_color_pairs{}
    {

        // In ncurses, colors are specified with: color pairs and color indices.
        //
        // A "color pair" is a foreground and background color. ncurses only alters
        // a cell's color with a color pair (we may not individually address its
        // foreground or background).
        //
        // Each color pair is a global integer, addressed by COLOR_PAIR(1) or COLOR_PAIR(42).
        //
        // We edit a color pair thusly:
        //     init_pair(color_pair, color_index_fg, color_index_bg);
        //
        // Note! COLOR_PAIR(0) is reserved and not editable.
        //
        // The foreground and background of a color pair are themselves
        // integers (indices into a global table of colors). These are "color
        // indices". They are specified by 3 values: red, green. and blue;
        // which range in saturation from 0 to 1000.
        //
        // We may edit the RGB at a color index but be aware
        // it will impact *all* color pairs which use it.
        //
        // And we edit a color index as follows:
        //     init_color(color_index, 500, 500, 500);

        /////////////////////////////////////////////////////////////////////

        // save existing color indices and color pairs
        for (int i = m_begin_gradient_color_pair; i <= m_highlight_color_pair; ++i)
        {
            short color_index = i;
            short r, g, b;
            color_content(color_index, &r, &g, &b);
            m_original_color_indices.insert({color_index, {r, g, b}});

            short color_pair = i;
            short fg, bg;
            pair_content(color_pair, &fg, &bg);
            m_original_color_pairs.insert({color_pair, {fg, bg}});
        }

        // To create the trailing drop effect, we must create a gradient.
        // In ncurses, this will be many color pairs, from least (invisible) to most
        // saturated with one additional "highlight" for drop's position.
        //
        // As color pair 0 is off limits for editing, we'll start the least
        // saturated part of the gradient at an offset, m_begin_gradient_color_pair

        // define black and white colors indices
        int const color_index_black = m_begin_gradient_color_pair;
        int const color_index_white = m_highlight_color_pair;
        init_color(color_index_black, 0, 0, 0);
        init_color(color_index_white, 700, 700, 700); // arbitrary saturation values

        // for each step in our gradient, ramp up the saturation
        int MAX_SATURATION = 1000;
        for (int i = 0; i < m_gradient_steps; ++i)
        {
            // define a color with these RGB values
            int color_index = m_begin_gradient_color_pair + i;
            int r = 0;
            int g = i * (MAX_SATURATION / m_gradient_steps);
            int b = 0;
            init_color(color_index, r, g, b);

            // define color pair with
            // foreground of this color index, and
            // background of black
            int color_pair = m_begin_gradient_color_pair + i;
            int fg = color_index;
            int bg = color_index_black;
            init_pair(color_pair, fg, bg);
        }

        // generate highlight color pair
        int fg = color_index_black;
        int bg = color_index_white;
        init_pair(m_highlight_color_pair, fg, bg);

        // fill window with least saturated color pair
        int invisible_gradient_color_pair = COLOR_PAIR(m_begin_gradient_color_pair);
        wbkgd(m_window, invisible_gradient_color_pair);
    }

    ~Rain()
    {
        // restore prior color indices
        for (auto const [color_index, rgb] : m_original_color_indices)
        {
            auto [r, g, b] = rgb;
            init_color(color_index, r, g, b);
        }

        // restore prior color pairs
        for (auto const [color_pair, pair] : m_original_color_pairs)
        {
            auto [fg, bg] = pair;
            init_pair(color_pair, fg, bg);
        }
    }

    // the ncurses window we're painting to
    WINDOW* m_window;
    // probability [0-1] (per frame) a new rain drop is added to a column
    double m_density;
    // probability [0-1] (per frame) a rain drop rolls to next row
    double m_roll_rate;
    // probability [0-1] (per frame) a cell fades
    double m_fade_rate;
    // RNG
    std::mt19937 m_generator;
    std::uniform_real_distribution<double> m_distribution;
    // character set
    std::vector<int> m_character_set;
    // because ncurses does not allow editing COLOR_PAIR(0), this is an offset
    // to where we may begin editing color pairs (for creating the gradient)
    int const m_begin_gradient_color_pair;
    // how many steps is our color pair gradient from invisible to full saturation
    int const m_gradient_steps;
    // which color pair is the highlight color pair
    // if our gradient ranges from color pair N to M this will be M+1
    int const m_highlight_color_pair;
    // original color indices
    std::map<short, std::tuple<short, short, short>> m_original_color_indices;
    // original color pairs
    std::map<short, std::pair<short, short>> m_original_color_pairs;

    void paint()
    {
        // get current bounds of window
        int rows;
        int cols;
        getmaxyx(m_window, rows, cols);

        // for each column of rain drops
        for (int col = 0; col < cols; ++col)
        {
            // for each row in column
            for (int row = 0; row < rows; ++row)
            {
                // if this row has a drop
                if (is_drop(row, col))
                {
                    // maybe roll drop downwards
                    if (should_roll_drop())
                    {
                        // fade prior position
                        fade(row, col);
                        // highlight next position
                        if (row + 1 < rows)
                        {
                            highlight(row + 1, col);
                        }
                    }
                }
                // else this is not a drop
                else if (should_fade())
                {
                    fade(row, col);
                }
            }
            // maybe add new drop if there's none at the top
            if (should_add_drop() and not is_drop(0, col))
            {
                highlight(0, col);
            }
        }
    }

private:

    void highlight(int row, int col)
    {
        mvwaddch(m_window, row, col, get_random_character() | COLOR_PAIR(m_highlight_color_pair));
    }

    void fade(int row, int col)
    {
        // get color pair of this cell
        chtype cell = mvwinch(m_window, row, col);
        short color_pair = PAIR_NUMBER(cell);
        // decrement (fade) color pair at this cell
        short faded_color_pair = color_pair > m_begin_gradient_color_pair ? color_pair - 1 : m_begin_gradient_color_pair;
        mvwchgat(m_window, row, col, 1, A_NORMAL, faded_color_pair, nullptr);
    }

    bool is_drop(int row, int col)
    {
        // get color pair of this cell
        chtype cell = mvwinch(m_window, row, col);
        // is it highlighted (a drop)?
        return m_highlight_color_pair == PAIR_NUMBER(cell);
    }

    int get_random_character()
    {
        size_t random_index = m_distribution(m_generator) * m_character_set.size();
        return m_character_set.at(random_index);
    }

    bool should_roll_drop()
    {
        return m_distribution(m_generator) < m_roll_rate;
    }

    bool should_fade()
    {
        return m_distribution(m_generator) < m_fade_rate;
    }

    bool should_add_drop()
    {
        return m_distribution(m_generator) < m_density;
    }

};

int main()
{
    // initialize ncurses
    initscr();
    raw();
    keypad(stdscr, true);
    noecho();
    curs_set(0);

    // exit if we cannot access colors nor modify them
    if (not has_colors() or not can_change_color())
    {
        std::string const message = "This terminal does not have access to color... exiting\n";
        mvprintw(
                LINES/2,
                COLS/2 - message.size()/2,
                message.c_str());
        getch();
        goto EXIT;
    }

    // turn on ncurses color
    // this sets COLORS and COLOR_PAIRS for us to check
    start_color();

    // exit if we do not have enough colors
    if(COLORS < 255 or COLOR_PAIRS < 255)
    {
        std::string const message = "This terminal does not have access to enough colors... exiting\n";
        mvprintw(
                LINES/2,
                COLS/2 - message.size()/2,
                message.c_str());
        getch();
        goto EXIT;
    }

    // set frame rate, how long to wait for new input
    timeout(80 /*milliseconds*/);

    {
        // initialize rain structure
        double density = 0.001;
        double roll_rate = 0.4;
        double fade_rate = 0.2;
        // printable ascii character set
        std::vector<int> character_set(93);
        std::iota(std::begin(character_set), std::end(character_set), 33);
        Rain rain
        {
            stdscr, // the main window of the screen that ncurses initializes
            density,
            roll_rate,
            fade_rate,
            character_set
        };

        while (true)
        {
            if (getch() == 'q')
            {
                break;
            }
            rain.paint();
        }
    }

    // release ncurses
EXIT:
    curs_set(1);
    echo();
    noraw();
    endwin();
}


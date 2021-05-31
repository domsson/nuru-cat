#define NURU_IMPLEMENTATION
#define NURU_SCOPE static

#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <stdint.h>     // uint8_t, uint16_t, ...
#include <ctype.h>      // tolower()
#include <unistd.h>     // getopt(), STDOUT_FILENO
#include <termios.h>    // struct winsize, struct termios, tcgetattr(), ...
#include <sys/ioctl.h>  // ioctl(), TIOCGWINSZ
#include <locale.h>     // setlocale(), LC_CTYPE
#include <wchar.h>      // fwide(), wchar_t
#include <limits.h>     // PATH_MAX (don't hit me)
#include "nuru.h"       // nuru minimal reference implementation

// program information

#define PROJECT_NAME "nuru"
#define PROGRAM_NAME "nuru-cat"
#define PROGRAM_URL  "https://github.com/domsson/nuru-cat"

#define PROGRAM_VER_MAJOR 0
#define PROGRAM_VER_MINOR 1
#define PROGRAM_VER_PATCH 0

// ANSI escape codes
// https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit

#define ANSI_FONT_RESET   L"\x1b[0m"
#define ANSI_FONT_BOLD    L"\x1b[1m"
#define ANSI_FONT_NORMAL  L"\x1b[22m"
#define ANSI_FONT_FAINT   L"\x1b[2m"

#define ANSI_HIDE_CURSOR  L"\e[?25l"
#define ANSI_SHOW_CURSOR  L"\e[?25h"

#define ANSI_CLEAR_SCREEN L"\x1b[2J"
#define ANSI_CURSOR_RESET L"\x1b[H"

typedef struct options
{
	char *nui_file;        // nuru image file to load
	char *nug_file;        // nuru glyph palette file to load
	char *nuc_file;        // nuru color palette file to load
	uint8_t info;          // print image info and exit
	uint8_t clear;         // clear terminal before printing
	uint8_t help : 1;      // show help and exit
	uint8_t version : 1;   // show version and exit
}
options_s;

/*
 * Parse command line args into the provided options_s struct.
 */
static void
parse_args(int argc, char **argv, options_s *opts)
{
	opterr = 0;
	int o;
	while ((o = getopt(argc, argv, "b:c:Cf:g:ihV")) != -1)
	{
		switch (o)
		{
			case 'c':
				opts->nuc_file = optarg;
				break;
			case 'C':
				opts->clear = 1;
				break;
			case 'g':
				opts->nug_file = optarg;
				break;
			case 'h':
				opts->help = 1;
				break;
			case 'i':
				opts->info = 1;
				break;
			case 'V':
				opts->version = 1;
				break;
		}
	}
	if (optind < argc)
	{
		opts->nui_file = argv[optind];
	}
}

/*
 * Print usage information.
 */
static void
help(const char *invocation, FILE *where)
{
	fprintf(where, "USAGE\n");
	fprintf(where, "\t%s [OPTIONS...] image_file\n\n", invocation);
	fprintf(where, "OPTIONS\n");
	fprintf(where, "\t-C\tclear the console before printing\n");
	fprintf(where, "\t-c FILE\tpath to color palette file to use\n");
	fprintf(where, "\t-g FILE\tpath to glyph palette file to use\n");
	fprintf(where, "\t-h\tprint this help text and exit\n");
	fprintf(where, "\t-i\tshow image information and exit\n");
	fprintf(where, "\t-V\tprint version information and exit\n");
}

/*
 * Print version information.
 */
static void
version(FILE *where)
{
	fprintf(where, "%s %d.%d.%d\n%s\n", PROGRAM_NAME,
			PROGRAM_VER_MAJOR, PROGRAM_VER_MINOR, PROGRAM_VER_PATCH,
			PROGRAM_URL);
}

/*
 * Print nuru image header information.
 */
void
info(nuru_img_s *img)
{
	fprintf(stdout, "signature:  %s\n", img->signature);
	fprintf(stdout, "version:    %d\n", img->version);
	fprintf(stdout, "color_mode: %d\n", img->color_mode);
	fprintf(stdout, "glpyh_mode: %d\n", img->glyph_mode);
	fprintf(stdout, "mdata_mode: %d\n", img->mdata_mode);
	fprintf(stdout, "cols:       %d\n", img->cols);
	fprintf(stdout, "rows:       %d\n", img->rows);
	fprintf(stdout, "ch_key:     %d\n", img->ch_key);
	fprintf(stdout, "fg_key:     %d\n", img->fg_key);
	fprintf(stdout, "bg_key:     %d\n", img->bg_key);
	fprintf(stdout, "glyph_pal:  %s\n", img->glyph_pal);
	fprintf(stdout, "color_pal:  %s\n", img->color_pal);
}

/*
 * Try to figure out the terminal size, in character cells, and return that 
 * info in the given winsize structure. Returns 0 on succes, -1 on error.
 * However, you might still want to check if the ws_col and ws_row fields 
 * actually contain values other than 0. They should. But who knows.
 */
static int
term_wsize(struct winsize *ws)
{
#ifndef TIOCGWINSZ
	return -1;
#endif
	return ioctl(STDOUT_FILENO, TIOCGWINSZ, ws);
}

/*
 * Turn echoing of keyboard input on/off.
 */
static int
term_echo(int on)
{
	struct termios ta;
	if (tcgetattr(STDIN_FILENO, &ta) != 0)
	{
		return -1;
	}
	ta.c_lflag = on ? ta.c_lflag | ECHO : ta.c_lflag & ~ECHO;
	return tcsetattr(STDIN_FILENO, TCSAFLUSH, &ta);
}

/*
 * Clear the entire terminal and move the cursor back to the top left.
 */
static void
term_clear()
{
	fputws(ANSI_CLEAR_SCREEN, stdout);
	fputws(ANSI_CURSOR_RESET, stdout);
}

/*
 * Prepare the terminal for our matrix shenanigans.
 */
static void
term_setup(options_s *opts)
{
	fputws(ANSI_HIDE_CURSOR, stdout);
	term_echo(0);                      // don't show keyboard input
	if (opts->clear) term_clear();     // if requested, clear terminal
}

/*
 * Make sure the terminal goes back to its normal state.
 */
static void
term_reset()
{
	fputws(ANSI_FONT_RESET, stdout);   // resets font colors and effects
	fputws(ANSI_SHOW_CURSOR, stdout);  // show the cursor again
	term_echo(1);                      // show keyboard input
}

static void 
print_color_4bit(nuru_cell_s* cell, uint8_t fg_key, uint8_t bg_key)
{
	if (cell->fg != fg_key)
	{
		// 0 =>  30, 1 =>  31, ...  7 =>  37
		// 8 =>  90, 8 =>  91, ... 15 =>  97
		uint8_t col = (cell->fg < 8 ? cell->fg + 30 : cell->fg + 82);
		wprintf(L"\x1b[%hhum", col);
	}
	if (cell->bg != bg_key)
	{
		// 0 =>  40, 1 =>  41, ...  7 =>  47
		// 8 => 100, 9 => 101, ... 15 => 107
		uint8_t col = (cell->bg < 8 ? cell->bg + 30 : cell->bg + 82) + 10;
		wprintf(L"\x1b[%hhum", col);
	}
}

static void
print_color_8bit(nuru_cell_s* cell, uint8_t fg_key, uint8_t bg_key)
{
	if (cell->fg != fg_key)
	{
		wprintf(L"\x1b[38;5;%hhum", cell->fg);
	}
	if (cell->bg != bg_key)
	{
		wprintf(L"\x1b[48;5;%hhum", cell->bg);
	}
}

static void
print_color_pal(nuru_cell_s *cell, uint8_t fg_key, uint8_t bg_key, nuru_pal_s* pal)
{
	if (cell->fg != fg_key)
	{
		if (pal->type == NURU_PAL_TYPE_COLOR_8BIT)
		{
			uint8_t col = nuru_pal_get_col_8bit(pal, cell->fg);
			wprintf(L"\x1b[38;5;%hhum", col);
		}

		else if (pal->type == NURU_PAL_TYPE_COLOR_RGB)
		{
			nuru_rgb_s* rgb = nuru_pal_get_col_rgb(pal, cell->fg);
			wprintf(L"\x1b[38;2;%hhu;%hhu;%hhum", rgb->r, rgb->g, rgb->b);
		}
	}

	if (cell->bg != bg_key)
	{
		if (pal->type == NURU_PAL_TYPE_COLOR_8BIT)
		{
			uint8_t col = nuru_pal_get_col_8bit(pal, cell->bg);
			wprintf(L"\x1b[48;5;%hhum", col);
		}

		else if (pal->type == NURU_PAL_TYPE_COLOR_RGB)
		{
			nuru_rgb_s* rgb = nuru_pal_get_col_rgb(pal, cell->bg);
			wprintf(L"\x1b[48;2;%hhu;%hhu;%hhum", rgb->r, rgb->g, rgb->b);
		}
	}
}

static void
print_glyph_none()
{
	wchar_t space = NURU_SPACE;
	fputwc(space, stdout);
}

static void
print_glyph_ascii(nuru_cell_s* cell, uint8_t ch_key)
{
	if (cell->ch == ch_key)
	{
		print_glyph_none();
		return;
	}
	fputwc((wchar_t) cell->ch, stdout);
}

static void
print_glyph_unicode(nuru_cell_s* cell, uint8_t ch_key)
{
	if (cell->ch == ch_key)
	{
		print_glyph_none();
		return;
	}
	fputwc((wchar_t) cell->ch, stdout);
}

static void
print_glyph_pal(nuru_cell_s* cell, uint8_t ch_key, nuru_pal_s* nug)
{
	if (cell->ch == ch_key)
	{
		print_glyph_none();
		return;
	}
	wchar_t ch = nuru_pal_get_glyph(nug, cell->ch);
	fputwc((wchar_t) ch, stdout);
}

static int
print_nui(nuru_img_s *nui, nuru_pal_s *nug, nuru_pal_s *nuc, uint16_t cols, uint16_t rows)
{
	nuru_cell_s *cell = NULL;

	for (uint16_t r = 0; r < nui->rows && r < rows; ++r)
	{
		for (uint16_t c = 0; c < nui->cols && c < cols; ++c)
		{
			cell = nuru_img_get_cell(nui, c, r);

			switch (nui->color_mode)
			{
				case NURU_COLOR_MODE_NONE:
					break;
				case NURU_COLOR_MODE_4BIT:
					print_color_4bit(cell, nui->fg_key, nui->bg_key);
					break;
				case NURU_COLOR_MODE_8BIT:
					print_color_8bit(cell, nui->fg_key, nui->bg_key);
					break;
				case NURU_COLOR_MODE_PALETTE:
					print_color_pal(cell, nui->fg_key, nui->bg_key, nuc);
					break;	
			}

			switch (nui->glyph_mode)
			{
				case NURU_GLYPH_MODE_NONE:
					print_glyph_none();
					break;
				case NURU_GLYPH_MODE_ASCII:
					print_glyph_ascii(cell, nui->ch_key);
					break;
				case NURU_GLYPH_MODE_UNICODE:
					print_glyph_unicode(cell, nui->ch_key);
					break;
				case NURU_GLYPH_MODE_PALETTE:
					print_glyph_pal(cell, nui->ch_key, nug);
					break;
			}
			fputws(ANSI_FONT_RESET, stdout);
		}	
		fputwc('\n', stdout);
	}
	
	return -1;
}

void
make_lower(char *str)
{
	for (int i = 0; str[i]; ++i)
	{
		str[i] = tolower(str[i]);
	}
}

int
pal_path(char *buf, size_t len, const char *pal, const char *type)
{
	char *home = getenv("HOME");
	char *config = getenv("XDG_CONFIG_HOME");

	if (config)
	{
		return snprintf(buf, len, "%s/%s/%s/%s.%s", 
				config, 
				PROJECT_NAME, 
				type, 
				pal, 
				NURU_PAL_FILEEXT
		);
	}
	else
	{
		return snprintf(buf, len, "%s/%s/%s/%s/%s.%s", 
				home, 
				".config", 
				PROJECT_NAME, 
				type, 
				pal, 
				NURU_PAL_FILEEXT
		);
	}
}

int
load_pal_by_name(nuru_pal_s *nup, const char *type, const char *name)
{
	// make a copy of the name and lower-case it
	char pal_name[NURU_STR_LEN];
	strcpy(pal_name, name);
	make_lower(pal_name);

	// get the full path to the palette
	char path[PATH_MAX];
	pal_path(path, PATH_MAX, pal_name, type);

	// load the palette
	return nuru_pal_load(nup, path) == 0 ? 0 : -1;
}

int
main(int argc, char **argv)
{
	// parse command line options
	options_s opts = { 0 };
	parse_args(argc, argv, &opts);

	if (opts.help)
	{
		help(argv[0], stdout);
		return EXIT_SUCCESS;
	}

	if (opts.version)
	{
		version(stdout);
		return EXIT_SUCCESS;
	}

	if (opts.nui_file == NULL)
	{
		fprintf(stderr, "No image file given\n");
		return EXIT_FAILURE;
	}

	// load nuru image file
	nuru_img_s nui = { 0 };
	if (nuru_img_load(&nui, opts.nui_file) == -1)
	{
		fprintf(stderr, "Error loading image file: %s\n", opts.nui_file);
		return EXIT_FAILURE;
	}

	if (opts.info)
	{
		info(&nui);
		return EXIT_SUCCESS;
	}

	// figure out if the image needs palette files
	uint8_t using_glyph_pal = (nui.glyph_mode & 128) && nui.glyph_pal[0];
	uint8_t using_color_pal = (nui.color_mode & 128) && nui.color_pal[0];

	// potentially load a glyph palette
	nuru_pal_s nug = { 0 };
	if (opts.nug_file)
	{
		if (nuru_pal_load(&nug, opts.nug_file) == -1)
		{
			fprintf(stderr, "Error loading palette file: %s\n", opts.nug_file);
			return EXIT_FAILURE;
		}
	}
	else if (using_glyph_pal)
	{
		if (load_pal_by_name(&nug, "glyphs", nui.glyph_pal) == -1)
		{
			fprintf(stderr, "Error loading palette: %s\n", nui.glyph_pal);
			return EXIT_FAILURE;
		}
	}

	// potentially load a color palette
	nuru_pal_s nuc = { 0 };
	if (opts.nuc_file)
	{
		if (nuru_pal_load(&nuc, opts.nuc_file) == -1)
		{
			fprintf(stderr, "Error loading palette file: %s\n", opts.nuc_file);
			return EXIT_FAILURE;
		}
	}
	else if (using_color_pal)
	{
		if (load_pal_by_name(&nuc, "colors", nui.color_pal) == -1)
		{
			fprintf(stderr, "Error loading palette: %s\n", nui.color_pal);
			return EXIT_FAILURE;
		}
	}
	
	// get the terminal dimensions
	struct winsize ws = { 0 };
	if (term_wsize(&ws) == -1)
	{
		fprintf(stderr, "Failed to determine terminal size\n");
		return EXIT_FAILURE;
	}

	if (ws.ws_col == 0 || ws.ws_row == 0)
	{
		fprintf(stderr, "Terminal size not appropriate\n");
		return EXIT_FAILURE;
	}

	// ensure unicode / wide-character support
	setlocale(LC_CTYPE, "");
	if (fwide(stdout, 1) != 1)
	{
		fprintf(stderr, "Couldn't put terminal in wide-character mode\n");
		return EXIT_FAILURE;
	}

	// display nuru image
	term_setup(&opts);
	print_nui(&nui, &nug, &nuc, ws.ws_col, ws.ws_row);

	// clean up and cya 
	nuru_img_free(&nui);
	term_reset();
	return EXIT_SUCCESS;
}

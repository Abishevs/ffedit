#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <ncurses.h>

#define PATH_SIZE 256
#define PAD_WIDTH 100

#define KB_i 'i'
#define KB_ESC 27

typedef enum { NORMAL, INSERT, COMMAND, } Mode;

typedef struct {
    char *data;         // Ptr to the array of characters
    size_t length;      // Current length of text
    size_t capacity;    // Maximum capacity before resizing
                        
    size_t vec_ptr;     // Cursor pos in buffer
    size_t col;
    size_t tmp_col;     // For saving col

    size_t row;
    size_t num_lines;
    size_t pad_y;

} TextBuffer;

typedef struct {
    char *str;
    size_t len;
    int left; // Left or right side?
} StatusBlock;

typedef struct {
    StatusBlock status_blocks; // Vec
    size_t num_blocks;
    size_t capacity;
} StatusLine;

/*  Utilis */
int get_last_row()
{
    return LINES - 2;
}

void set_block_cursor() {
    printf("\e[2 q"); // Block cursor
    fflush(stdout);
}

void set_thin_cursor() {
    printf("\e[5 q"); // Thin cursor |
    fflush(stdout);
}

int load_file_into_buffer(const char *file_name, TextBuffer *buf) 
{
    // Try to open the file
    FILE *file = fopen(file_name, "r");

    if (!file) {
        // If the file doesn't exist or can't be opened, initialize an empty buffer
        buf->data = malloc(1); // Allocate at least 1 byte
        if (!buf->data) {
            perror("Failed to allocate memory for an empty buffer");
            return -1;
        }
        buf->data[0] = '\0';   // Null-terminate
        buf->length = 0;
        buf->capacity = 1;    // Start small and grow dynamically as needed
        buf->col = 0;
        return 0; // Return success (empty document)
    }

    // File exists, determine the size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    // Allocate memory for the file content
    buf->data = malloc(file_size + 1); // +1 for null-terminator
    if (!buf->data) {
        perror("Failed to allocate memory");
        fclose(file);
        return -1;
    }

    // Read the file content into the buffer
    size_t read_size = fread(buf->data, 1, file_size, file);
    buf->data[read_size] = '\0'; // Null-terminate, even if the read is partial

    // Set metadata
    buf->length = read_size;
    buf->capacity = file_size + 1;
    buf->col = 0;

    fclose(file);
    return 0; // Success
}

void save_file(TextBuffer *buf, const char *file_name) 
{
    FILE *fptr;

    fptr = fopen(file_name, "w");

    fprintf(fptr, "%s", buf->data);

    fclose(fptr);
}

const char *mode_to_str(Mode mode)
{
    switch (mode) {
        case NORMAL: return "NORMAL";
        case INSERT: return "INSERT";
        case COMMAND: return "COMMAND";
        default: return "INVALID CMD";
    }
}


// Try to open file Return NULL if not found
void build_path(char *file_name, char *argv)
{
    // TODO: Add support for abs, ~ and  / paths
    // NOTE: Works rn for relative paths only
    int length = sprintf(file_name, "./%s", argv);
    printf("New string len: %d\n", length);
}

void count_num_lines(TextBuffer *buf)
{
    buf->num_lines = 0;
    for (size_t i = 0; i < buf->length; i++) {
        if (buf->data[i] == '\n') {
            buf->num_lines++;
        }
    }
}
void render_text(WINDOW *pad, TextBuffer *buf) {
    werase(pad); //Clear screen
                 
    int row = 0, col = 0;

    for (size_t i = 0; i < buf->length; i++) {
        if (buf->data[i] == '\n') {
            row++;
            col = 0;
        } else {
            mvwaddch(pad, row, col, buf->data[i]);
            col++;
        }
    }
}

int get_char_offset(TextBuffer *buf)
{
    int prev_lines = 0;
    int chars = 0;
    while (buf->row - prev_lines > 0) {
        if (buf->data[chars] == '\n') {
            prev_lines++;
        }
        chars++;
    }

    buf->vec_ptr = chars + buf->col;
    return chars;
}

int delete_char(TextBuffer *buf) 
{
    if (buf->length == 0 || buf->col + get_char_offset(buf) == 0) {
        return 0; // Nothing to delete
    } 

    size_t index = buf->col + get_char_offset(buf);

    if (index > 0) {
        /* Shift characters left */
        memmove(&buf->data[index - 1], &buf->data[index], buf->length - index);
    }

    buf->length--;

    /* Shrink memory only if buffer is large enough */
    if (buf->length < buf->capacity / 2) {
        char *tmp_ptr = realloc(buf->data, buf->length);
        if (tmp_ptr) {
            buf->data = tmp_ptr;
            buf->capacity = buf->length; // Adjust capacity correctly
        }
    }

    return 1;
}


int insert_char(TextBuffer *buf, char ch)
{  

/*
 *  It gets a index (cursor?) in array.
 *  1.  check bounds
 *  2.  Add ch to the curr cursor pos.
 *  2.1 Shift index++ nth elemnts to the right. 
 *  3.  Allocate more mem for the new char?
  */
    // TODO: Check bounds
    char *tmp_ptr = realloc(buf->data, buf->capacity + 1);
    if (tmp_ptr == NULL) {
        fprintf(stdin, "Failed to reallocate memory");
    }
    buf->data = tmp_ptr;
    buf->capacity++;

        
    size_t index = buf->col+get_char_offset(buf);

    memmove(&buf->data[index + 1], &buf->data[index], buf->length - index);


    buf->data[index] = ch;
    buf->length++;

    return 1;
}

void setup_terminal()
{
    initscr(); // Cursor mode
    raw();
    noecho();
    keypad(stdscr, TRUE);
    scrollok(stdscr, FALSE); 
    set_escdelay(25);     
    nodelay(stdscr, TRUE);
    clear();
}

void cleanup() {
    noraw();          
    echo();           
    endwin();         
    printf("\e[0 q"); // Reset cursor to default (if modified)
    fflush(stdout);
}


void draw_ui(TextBuffer *buf, Mode mode)
{
    int last_row = get_last_row();
    char *row_col_str = malloc(4);
    
    if (row_col_str == NULL) {
        //TODO: Handle Allocation error
        exit(1);
    }
    int row_col_len;
    row_col_len = sprintf(row_col_str, "%zu:%zu", buf->row + 1, buf->col + 1);

    if (row_col_len > 3) {
        row_col_str = realloc(row_col_str, row_col_len + 1); //+1 NULL terminator
        if (row_col_str == NULL) {
            //TODO: Handle Realloction error
            exit(1);
        }
    }

    // Status Line
    move(last_row, 0);
    clrtoeol();
    // Left
    start_color(); //WARN: Should be checked with
                   //has_colors() if term supports colors
                   
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
	attron(COLOR_PAIR(1));

    for (int col = 0; col < COLS; col++){
        mvprintw(last_row, col," "); 
    }
    mvprintw(last_row, 1,"%s", mode_to_str(mode)); 

    // Right
    mvprintw(last_row, COLS - 1 - row_col_len, "%s", row_col_str); 
    attroff(COLOR_PAIR(1));


    if (mode == COMMAND){
        // Command line
        move(last_row + 1, 0);
        clrtoeol();
        mvprintw(last_row + 1, 0, ":");  
    }

    free(row_col_str);
}

Mode handle_normal_mode(TextBuffer *buf, int ch, Mode mode)
{
    int tmp_col = buf->col;
    buf->col = 0;

    get_char_offset(buf);

    int cur_line_len = 0;
    int next_line_len = 0;
    int index = buf->vec_ptr;
    int lines = 2;

    while (lines > 0){
        if (buf->data[index] == '\n'){
            lines--;
            index++;
            continue;
        } else if (buf->data[index] == '\0'){
            break; // End of buf 
        }

        if (lines == 2) {
            cur_line_len++;
        } else next_line_len++; 
        index++;
    }

    index = buf->vec_ptr - 2;
    int prev_line_len = 0; // Will be exactly the line len
                           // Unlike cur and next
    while (buf->data[index] != '\n' && index > 0) {
        prev_line_len++;
        index--;
    }

    if (cur_line_len > 0) cur_line_len--;
    if (next_line_len > 0) next_line_len--;

    buf->col = tmp_col;

    move(LINES - 1, 0);
    clrtoeol();
    mvprintw(LINES - 1, 0, "prev: %d Cur:%d Next: %d",
            prev_line_len, cur_line_len, next_line_len);

    int visible_height = LINES - 2;
    int display_row = buf->row - buf->pad_y;

    if (ch == KB_i) {
        set_thin_cursor();
        return INSERT;
    } else if ( ch == ':' ) {
        return COMMAND;

    } else if (ch == 'h') { // Move left
        if (buf->col > 0) {
            buf->col--; 
            buf->tmp_col = buf->col;
        }

    } else if (ch == 'l') { // Move right
        if (buf->col < COLS - 1 && buf->col < cur_line_len) {
            buf->col++; 
            buf->tmp_col = buf->col;
        }

    } else if (ch == 'j') { // Move down
        if (buf->row < buf->num_lines) {
            buf->row++;

            /* Longest line so far save it */
            if ( buf->col > buf->tmp_col) {
                buf->tmp_col = buf->col;
            } 

            if (buf->tmp_col >= next_line_len) {
                buf->col = next_line_len;
            } else {
                buf->col = buf->tmp_col;
            }

            display_row = buf->row - buf->pad_y;

            if (display_row >= visible_height) {
                buf->pad_y++;  // Move the pad view down
            }
        }
    }  else if (ch == 'k') { // Move up
        if (buf->row > 0) {  // If not at the first line
            buf->row--;

            /* Longest line so far save it */
            if ( buf->col > buf->tmp_col) {
                buf->tmp_col = buf->col;
            } 

            if (buf->tmp_col >= prev_line_len) {
                buf->col = prev_line_len;
            } else {
                buf->col = buf->tmp_col;
            }

            display_row = buf->row - buf->pad_y;  // Recalculate visible row

            // Scroll up only if cursor is at the top of the visible area
            if (display_row < 0) {
                buf->pad_y--;  // Move the pad view up
            }
        }
    }

    return mode;

}

void handle_insert_mode(TextBuffer *buf, int ch)
{
    if (ch == '\n' || ch == KEY_ENTER || ch == '\r' || ch == 10) {
        insert_char(buf, ch);
        buf->row++;
        buf->col = 0;
    } else {
        if ( ch == KEY_BACKSPACE || ch == 263 ) {
            if (buf->col > 0) {
                delete_char(buf);
                buf->col--;
            } else if (buf->row > 0) {

                /* Number of chars in previous line excluding '\n' */
                int chars = 0;
                int index = get_char_offset(buf) - 1;

                /* Calc count of chars in prev line excluding \n */
                while (index - 1 >= 0 && buf->data[index - 1] != '\n') {
                    chars++;
                    index--;
                }
                /* Delete the character */
                delete_char(buf);

                /* Update cursor position */
                buf->col = chars;
                buf->row--;


            }
        } else {
            insert_char(buf, ch);
            buf->col++;
        }
    }
}

void cmd_parser(TextBuffer *buf, char *tmp_buf, const char *file_name, int *quit)
{
        if ( tmp_buf[0] == 'w' && tmp_buf[1] == 'q' ){
            save_file(buf, file_name);
           *quit = 1;
        } else if (tmp_buf[0] == 'q' ) {
            *quit = 1;
        } else if (tmp_buf[0] == 'w' ) {
            save_file(buf, file_name);
            int str_len = sprintf(tmp_buf,"'%s', %zuL, %zuB written",file_name, buf->num_lines, buf->length);
            //WARN: Reallocate aka look at size of the str
        }
}

Mode handle_command_mode(TextBuffer *buf, int ch, const char *file_name, int *quit)
{
    size_t capacity = 16;
    char *tmp_buf = malloc(capacity);
    if (!tmp_buf) return NORMAL;

    size_t buf_len = 0;
    int col = 1;
    int row = get_last_row() + 1;
    refresh();
    move(row, col);

    for (;;) {
        ch = getch();

        if (ch == KB_ESC) {
            move(row, 0);
            clrtoeol();
            break;
        }
        if (ch == '\n' || ch == KEY_ENTER || ch == '\r' || ch == 10) {
            // Handle command
            tmp_buf[buf_len] = '\0';  // Ensure null termination
            cmd_parser(buf, tmp_buf, file_name, quit);
            mvprintw(row, 0, "%s", tmp_buf);
            break;
        }

        if (buf_len + 1 >= capacity) { // +1 for null terminator
            size_t new_capacity = capacity * 2;
            char *new_buf = realloc(tmp_buf, new_capacity);
            if (!new_buf) {
                free(tmp_buf);
                return NORMAL;
            }
            tmp_buf = new_buf;
            capacity = new_capacity;
        }

        tmp_buf[buf_len++] = ch;
        tmp_buf[buf_len] = '\0';
        mvprintw(row, 1, "%s", tmp_buf);
    }

    free(tmp_buf);
    return NORMAL;
}

int number_length(int n) {
    int len = 0;
    if (n == 0) return 1; 

    n = abs(n);
    while (n > 0) {
        n /= 10;
        len++;
    }
    return len;
}

WINDOW *resize_pad(WINDOW *old_pad, size_t num_lines) 
{
    if (old_pad) delwin(old_pad);
    return newpad(num_lines > LINES ? num_lines : LINES, PAD_WIDTH);
}

int main(int argc, char *argv[])
    // Argc hold num of args
    // argv is ptr to string passed in
    // Argv[0] = program name 
{
    // TODO: Dynamcly allocate mem for path?
    TextBuffer buf = {0}; 
    char file_name[PATH_SIZE];
    Mode mode = NORMAL; // Start in normal mode
    int quit = 0;
    int current_pad_height = buf.num_lines;
    int display_row = 0;
    int ch;

    if (argc == 1) {
        // That means there are no arg passed in
        // TODO: Open a welcomepage?
        printf("Help msg\n");
        return 1;
    }

    build_path(file_name, argv[1]);
    load_file_into_buffer(file_name, &buf);
    count_num_lines(&buf);


    setup_terminal();

    WINDOW *pad = resize_pad(NULL, buf.num_lines);
    if (!pad) {
        endwin();
        printf("Failed to create pad\n");
        return 1;
    }

    WINDOW *side_win = newwin(LINES - 2, 3, 0, 0);
    if (!side_win) {
        endwin();
        printf("Failed to create sidebar window\n");
        return 1;
    }

    atexit(cleanup); // Register cleanup for when exit() called

    int sidebar_len = number_length(buf.num_lines);

    while (!quit) {
        draw_ui(&buf, mode);
        count_num_lines(&buf);
        render_text(pad, &buf);
        if (buf.num_lines > current_pad_height) {
            pad = resize_pad(pad, buf.num_lines);
            current_pad_height = buf.num_lines;
        }


        werase(side_win);
        for (int i = buf.pad_y; i < buf.pad_y + (LINES - 2) && i < buf.num_lines + 1; i++) {
            // BUG: Reverse the line_num :D
            mvwprintw(side_win, i - buf.pad_y, 0, "%d", i + 1);  // Line numbers
        }
        wrefresh(side_win);

        display_row = buf.row - buf.pad_y;
        if (display_row >= LINES - 2) display_row = LINES - 2;
        if (display_row < 0) display_row = 0;


        prefresh(pad, buf.pad_y, 0, 0, sidebar_len + 1, LINES - 2, COLS - 1);
        move(display_row, buf.col + sidebar_len + 1);


        if ( mode == COMMAND) {
            nodelay(stdscr, FALSE);  
            mode = handle_command_mode(&buf, ch, file_name, &quit);
            nodelay(stdscr, TRUE);
        } else {
            ch = getch();
            if (ch != ERR) {

                switch (mode) {
                    case NORMAL:
                        mode = handle_normal_mode(&buf, ch, mode);
                        break;
                    case INSERT:
                        if (ch == KB_ESC) {
                            mode = NORMAL;
                            set_block_cursor();
                        } else {
                            handle_insert_mode(&buf, ch);
                        }
                        break;
                    case COMMAND:
                        // Handled earlier
                        break;
                }

            }
        }
        usleep(16666); // ~60 fps, 16.6...ms
    }


    /*  Clean up */
    delwin(pad);
    cleanup();
    free(buf.data);


    printf("Terminal size: %d rows, %d cols\n", LINES, COLS);
    printf("Filename: %s\n", file_name);
    printf("Num of lines: %zu\n", buf.num_lines);

    
    return 0;
}

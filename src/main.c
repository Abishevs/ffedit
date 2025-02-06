#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

#define PATH_SIZE 256

#define KB_i 'i'
#define KB_ESC 27

typedef enum { NORMAL, INSERT, COMMAND, } Mode;

typedef struct {
    char *data;         // Ptr to the array of characters
    size_t length;      // Current length of text
    size_t capacity;    // Maximum capacity before resizing
    size_t col;      // Current cursor position
    size_t row;
                        // COL
    size_t num_lines;   // ROW, 
} TextBuffer;

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

void render_text(TextBuffer *buf) 
{
    int row = 0, col = 0;
    buf->num_lines = 0;
    
    for (size_t i = 0; i < buf->length; i++) {
        if (buf->data[i] == '\n') {
            row++;
            col = 0; // Reset column at newline
            buf->num_lines++;
        } else {
            clrtoeol();
            mvaddch(row, col, buf->data[i]);  // Draw character
            col++;
            if (col >= COLS) { // Wrap to next line if screen width is exceeded
                row++;
                col = 0;
            }
            if (row >= LINES - 2) {
                break;
            }
        }
    }
}

int get_char_offset(TextBuffer *buf)
{
    int prev_line = 0;
    int chars = 0;
    while (buf->row - prev_line > 0) {
        if (buf->data[chars] == '\n') {
            prev_line++;
        }
        chars++;
    }
    return chars;
}

int delete_char(TextBuffer *buf) 
{
    if (buf->length == 0 || buf->col == 0) {
        return 0; // Nothing to delete
    }

    size_t index = buf->col + get_char_offset(buf);

    char *tmp_ptr = realloc(buf->data, buf->capacity - 1);
    if (tmp_ptr == NULL) {
        fprintf(stdin, "Failed to reallocate memory");
    }
    buf->data = tmp_ptr;
    buf->capacity--;

    if (index > 0) {
        memmove(&buf->data[index - 1], &buf->data[index], buf->length - index);
    }


    buf->data[index] = buf->data[index + 1]; //Overwrite cur char with next
                                             //char

    buf->length--;
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
    nodelay(stdscr, FALSE);
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
        // Status Line
        move(get_last_row(), 0);
        clrtoeol();
        int last_row = get_last_row();
        mvprintw(last_row, 0,"%s", mode_to_str(mode)); 
        mvprintw(last_row, strlen(mode_to_str(mode)) + 1, "%zu:", buf->row); 
         // TODO: Make smarter function for printing row col, works with single
         // digits only rn.
        mvprintw(last_row, strlen(mode_to_str(mode)) + 3, "%zu", buf->col);  

        // Command line
        move(last_row + 1, 0);
        clrtoeol();
        mvprintw(last_row + 1, 0, ">");  
}

Mode handle_normal_mode(TextBuffer *buf, int ch, Mode mode)
{
    if (ch == KB_i) {
        set_thin_cursor();
        return INSERT;
    } else if ( ch == ':' ) {
        return COMMAND;
    } else if (ch == 'h') { // Move left
        if (buf->col > 0) {
            buf->col--; // Ensure we don't go out of bounds
        }

    } else if (ch == 'l') { // Move right
        if (buf->col < COLS - 1) {
            buf->col++; // Ensure we stay within screen width
        }

    } else if (ch == 'j') { // Move down
        if (buf->row + 1 < buf->num_lines ) buf->row++; // Ensure we stay within Status
                                                        // TODO: find next \n then go col

    } else if (ch == 'k') { // Move up
        if (buf->row > 0) buf->row--; // Ensure we don't go above the top
                                      // TODO: find prev \n then go col
    }

    return mode;

}

void handle_insert_mode(TextBuffer *buf, int ch)
{
    if (ch == '\n' || ch == KEY_ENTER || ch == '\r' || ch == 10) {
        buf->row++;
        insert_char(buf, ch);
        buf->col = 0;
    } else {
        if ( ch == KEY_BACKSPACE || ch == 263 ) {
            if ( buf->col > 0 ) {
                delete_char(buf);
                buf->col--;
            }
        } else {
            insert_char(buf, ch);
            buf->col++;
        }
    }
}


int handle_command_mode(TextBuffer *buf, int ch, const char *file_name, int quit)
{
    if ( ch == 'q' ){
        return 1;
    } else if ( ch == 'w' ) {
        save_file(buf, file_name);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
    // Argc hold num of args
    // argv is ptr to string passed in
    // Argv[0] = dir from which main called
{
    // TODO: Dynamcly allocate mem for path?
    TextBuffer buf = {0}; 
    char file_name[PATH_SIZE];
    Mode mode = NORMAL; // Start in normal mode
    int ch;
    int quit = 0;

    if (argc == 1) {
        // That means there are no arg passed in
        // TODO: Open a welcomepage?
        printf("Help msg\n");
        return 1;
    }

    build_path(file_name, argv[1]);
    load_file_into_buffer(file_name, &buf);
    setup_terminal();

    atexit(cleanup); // Register cleanup for when exit() called

    while (!quit) {
        draw_ui(&buf, mode);
        render_text(&buf);
        refresh();
        move(buf.row, buf.col); // Move cursor to start pos

        ch = getch(); // Blocking
        if (ch == KB_ESC) {
            mode = NORMAL;
            set_block_cursor();
        }

        switch (mode) {
            case NORMAL: 
                mode = handle_normal_mode(&buf, ch, mode);
                break;
            case INSERT: 
                handle_insert_mode(&buf, ch);
                break;
            case COMMAND:
                quit = handle_command_mode(&buf, ch, file_name, quit);
                break;
        }
        move(buf.row, buf.col);
    }


    /*  Clean up */
    cleanup();
    free(buf.data);


    printf("Terminal size: %d rows, %d cols\n", LINES, COLS);
    printf("Filename: %s\n", file_name);
    printf("Num of lines: %zu\n", buf.num_lines);

    
    return 0;
}

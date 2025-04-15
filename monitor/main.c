#include <ncurses.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#define MAX_SYSTEM_USAGE_SIZE 8192  // Define max buffer size for system usage, 2^16
#define MAX_LOG_SIZE 16777216  // Define max buffer size for logs, 2^24

// Window structure with origin, width, height, and depth
typedef struct {
    int x, y;
    int width, height;
    uint id;
	char* data; // Text to show in the window
	char* title;
    int scrollDownOffset;
} Window;

typedef struct {
	uint_fast32_t character;
	char controlFG[8], controlBG[8];
} Character; 

Window* windows = NULL;
uint numWindows = 0;
int focusedWindowIndex = 0; // Initially focused on the first window
uint_fast32_t prevMaxTermX, prevMaxTermY;
uint_fast32_t maxTermX, maxTermY;

Character **screenBuffer = NULL;  // 2D array for the virtual screen buffer

// Everything that is ssh
char* serverIPv4 = NULL;
char* serverUser = NULL;
char* sshKeyPath = NULL;
char* mcServerHome = NULL;
char localIP[16];

// General control variables
int exitError;
char running = 1;
char autoScroll = 1;
char silentMode = 0;

// Variables to lay down PIPE
static int x = -1, y = -1, dir = 0, iteration = 0;
static char* data = NULL;

char* read_file_as_string(const char* filename) {
    FILE* file = fopen(filename, "rb");  // Open file in binary mode
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    uint_fast64_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate memory for the file content
    char* content = (char*)malloc(file_size + 1);  // +1 for null-terminator
    if (!content) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    // Read the file into the buffer
    fread(content, 1, file_size, file);
    content[file_size] = '\0';  // Null-terminate the string

    fclose(file);
    return content;
}

int draw_border_to_buffer(uint windowsIndex) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x); // Get terminal size

    if (windows[windowsIndex].x >= max_x || windows[windowsIndex].y >= max_y) {
        return 1; // Skip drawing if out of bounds
    }

    char** lines = NULL;
    long long int line_count = 0;

    if (windows[windowsIndex].data) {
        char* data_copy = strdup(windows[windowsIndex].data);
        char* token = strtok(data_copy, "\n");
        while (token) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            lines[line_count++] = strdup(token);
            token = strtok(NULL, "\n");
        }
        free(data_copy);
    }

    // Apply scroll offset to skip lines
    uint scroll_offset = windows[windowsIndex].scrollDownOffset;
    if (scroll_offset >= line_count) scroll_offset = line_count - 1;  // Cap offset
    if (scroll_offset < 0) scroll_offset = 0;

    for (uint_fast32_t i = 0; i < windows[windowsIndex].height; i++) {
        int current_y = windows[windowsIndex].y + i;
        if (current_y >= max_y) break;

        int last_char_index = windows[windowsIndex].width - 3;

        for (uint_fast32_t j = 0; j < windows[windowsIndex].width; j++) {
            if (windows[windowsIndex].x + j < 0 || windows[windowsIndex].x + j >= max_x) continue;

            if (i == 0) { // Draw top border or title
                if (j == 0) {
                    screenBuffer[current_y][windows[windowsIndex].x + j].character = ACS_ULCORNER;
                } else if (j == windows[windowsIndex].width - 1) {
                    screenBuffer[current_y][windows[windowsIndex].x + j].character = ACS_URCORNER;
                } else {
                    screenBuffer[current_y][windows[windowsIndex].x + j].character =
                        (windows[windowsIndex].title && j - 1 < (uint)strlen(windows[windowsIndex].title))
                            ? windows[windowsIndex].title[j - 1]
                            : ACS_HLINE;
                }
            } else if (i == windows[windowsIndex].height - 1) { // Draw bottom border
                if (j == 0) {
                    screenBuffer[current_y][windows[windowsIndex].x + j].character = ACS_LLCORNER;
                } else if (j == windows[windowsIndex].width - 1) {
                    screenBuffer[current_y][windows[windowsIndex].x + j].character = ACS_LRCORNER;
                } else {
                    screenBuffer[current_y][windows[windowsIndex].x + j].character = ACS_HLINE;
                }
            } else { // Draw content or vertical borders
                uint_fast32_t line_index = i - 1 + scroll_offset;
                uint_fast32_t char_index = j - 1;

                if (j == 0 || j == windows[windowsIndex].width - 1) {
                    screenBuffer[current_y][windows[windowsIndex].x + j].character = ACS_VLINE;
                } else if (line_index < line_count && lines[line_index]) {
                    if (char_index < last_char_index && char_index < (uint)strlen(lines[line_index])) {
                        screenBuffer[current_y][windows[windowsIndex].x + j].character = lines[line_index][char_index];
                    } else if (char_index == last_char_index && (uint)strlen(lines[line_index]) > last_char_index) {
                        screenBuffer[current_y][windows[windowsIndex].x + j].character = '>';
                        screenBuffer[current_y][windows[windowsIndex].x + j - 1].character = ' ';
                    } else {
                        screenBuffer[current_y][windows[windowsIndex].x + j].character = ' ';
                    }
                } else {
                    screenBuffer[current_y][windows[windowsIndex].x + j].character = ' ';
                }
            }
        }
    }

    // Free allocated memory for lines
    for (int i = 0; i < line_count; i++) free(lines[i]);
    free(lines);

    return 1;
}

// Third window had this, similar to pipes.sh (not mine), but I commented out the third here
/*
void generate_pipes_data() {
    int width = windows[2].width - 3; // Exclude borders
    int height = windows[2].height - 2;

    // Pipe characters
    char pipes[4] = {'|', '-', '|', '-'};

    // Directions: 0 = down, 1 = right, 2 = up, 3 = left
    int dx[4] = {0, 1, 0, -1};
    int dy[4] = {1, 0, -1, 0};

    // Initialize/reset pipe data if needed
    if (iteration == 0 || x < 0 || y < 0 || !data) {
        // Allocate/reset data buffer
        free(data);
        data = (char*)calloc((width + 1) * height + 1, sizeof(char));

        // Clear buffer with spaces
        for (int i = 0; i < height; i++) {
            memset(data + i * (width + 1), ' ', width);
            data[i * (width + 1) + width] = '\n';
        }
        data[(width + 1) * height] = '\0';

        // Start at the center
        x = width / 2;
        y = height / 2;
        dir = rand() % 4;
        iteration = 0;
    }

    // Check bounds before drawing
    if (x >= 0 && x < width && y >= 0 && y < height) {
        // Draw the current segment if the cell is empty
        if (data[y * (width + 1) + x] == ' ') {
            data[y * (width + 1) + x] = pipes[dir];
        }
    }

    // Choose a random bend direction occasionally
    if (rand() % 5 == 0) {
        dir = (dir + (rand() % 3 - 1) + 4) % 4;
    }

    // Move to the next position
    int new_x = x + dx[dir];
    int new_y = y + dy[dir];

    // Check if the next position is within bounds
    if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
        x = new_x;
        y = new_y;
    } else {
        // If out of bounds, choose a new random direction
        dir = rand() % 4;
    }

    // Update window data
    free(windows[2].data);
    windows[2].data = strdup(data);

    // Increment iteration count and reset after 50 iterations
    iteration++;
    if (iteration >= 600) {
        iteration = 0;
        free(data);
        data = NULL;
    }
}

// Compare function for qsort to sort by z order
int compare_z(const void* a, const void* b) {
    Window* winA = (Window*)a;
    Window* winB = (Window*)b;
    return winA->z - winB->z; // Ascending order
}*/

void update_screen() {
    // Update only the parts that have changed
    for (uint i = 0; i < maxTermY; i++) {
        for (uint j = 0; j < maxTermX; j++) {
			move(i, j);
			addch(screenBuffer[i][j].character);
        }
    }
    refresh();
}

int userInput() {
    int keyPressed = getch();
    // Check for number for window switching
    if (keyPressed >= 1 && keyPressed <= 9) {
        int windowIndex = keyPressed - 1; // Convert to 0-based index
        if (windowIndex + 1 < numWindows) {
            focusedWindowIndex = windowIndex;
        }
    } 
    // Handle Up Arrow (scroll up)
    else if (keyPressed == KEY_UP) {
        if (windows[focusedWindowIndex].scrollDownOffset > 0) {
            windows[focusedWindowIndex].scrollDownOffset--; // Scroll up by reducing the offset
            autoScroll = 0;
        }
    }
    // Handle Down Arrow (scroll down)
    else if (keyPressed == KEY_DOWN) {
        // Ensure that scrollDownOffset does not exceed the number of available lines in the window
        int max_lines = windows[focusedWindowIndex].data ? (int)strlen(windows[focusedWindowIndex].data) : 0;
        if (windows[focusedWindowIndex].scrollDownOffset < max_lines) {
            windows[focusedWindowIndex].scrollDownOffset++; // Scroll down by increasing the offset
            autoScroll = 0;
        }
    }
    else if (keyPressed == '\n') {
		autoScroll = 1;
	}
    
    // Handle Escape (Exit)
    else if (keyPressed == 0x1B) { // Escape to exit
        running = 0; // Exit cleanly
    }

    return 1;
}

void scrollToBottom(uint windowsIndex) {
    if (windowsIndex >= numWindows) return;  // Invalid window index

    // Get the number of lines in the window's data
    int line_count = 0;
    if (windows[windowsIndex].data) {
        char* data_copy = strdup(windows[windowsIndex].data);
        char* token = strtok(data_copy, "\n");
        while (token) {
            line_count++;
            token = strtok(NULL, "\n");
        }
        free(data_copy);
    }

    // Set the scroll offset to the bottom, effectively scrolling the window to the bottom
    windows[windowsIndex].scrollDownOffset = line_count - windows[windowsIndex].height + 2;
    if (windows[windowsIndex].scrollDownOffset < 0) {
        windows[windowsIndex].scrollDownOffset = 0;  // Ensure it doesn't go negative
    }
}

int create_window(int x, int y, int width, int height, char* title) {
    numWindows++;
    
    // Reallocate memory for the new window array
    windows = (Window*)realloc(windows, numWindows * sizeof(Window));
    if (!windows) {
        return 0; // Memory allocation failed
    }
    
    uint id = numWindows;
    windows[numWindows - 1] = (Window){x, y, width, height, id, NULL, title};
    
    return 1; // Success
}

void resize_windows() {
	// Resize the screen buffer to match terminal size
    screenBuffer = (Character**)realloc(screenBuffer, maxTermY * sizeof(Character*));
    for (int i = 0; i < maxTermY; i++) {
        screenBuffer[i] = (Character*)realloc(screenBuffer[i], maxTermX * sizeof(Character));
    }
    
    // Clear screen buffer before redrawing
	for (unsigned char i = 0; i < maxTermY; i++) {
	    for (unsigned char j = 0; j < maxTermX; j++) {
	        screenBuffer[i][j].character = 0x20; 
	    }
	}
	
    
    for (unsigned char i = 0; i < numWindows; i++) {
        // Resize window to fit within terminal bounds
        if (i == 0) { // hardcoding go brrr
			windows[0].width = maxTermX/2 + maxTermX/6 - 3;
			windows[0].height = ceil(maxTermY) - 2; 
		} else if (i == 1) {
			windows[i].width = maxTermX/2 - maxTermX/6 - 3;
			windows[i].height = ceil(maxTermY) - 2;
			
			windows[i].x = maxTermX/2 + maxTermX/6;
		} /*else {
			windows[i].width = maxTermX/2 - maxTermX/6 - 3;
			windows[i].height = ceil((float)maxTermY/2);
			
			windows[i].x = maxTermX/2 + maxTermX/6;
			windows[i].y = maxTermY/2 - 1;
		}*/
    }
}

char* run_ssh_command(size_t maxSize, const char* command, ...) {
    char formattedCommand[512];
    va_list args;
    
    // Process variadic arguments and format the command
    va_start(args, command);
    vsnprintf(formattedCommand, sizeof(formattedCommand), command, args);
    va_end(args);

    char sshCommand[512];
    snprintf(sshCommand, sizeof(sshCommand),
             "ssh -i %s -p 22 %s@%s '%s'",
             sshKeyPath, serverUser, serverIPv4, formattedCommand);

    FILE* fp = popen(sshCommand, "r");
    if (!fp) return NULL;

    char* result = (char*)malloc(maxSize);
    if (!result) return NULL;

    size_t offset = 0;
    while (fgets(result + offset, maxSize - offset, fp) != NULL) {
        offset += strlen(result + offset);
        if (offset >= maxSize) {
            break;
        }
    }
    pclose(fp);
    return result;
}

int checkSSHConnection(int timeout) {
    char sshCommand[512];

    snprintf(sshCommand, sizeof(sshCommand),
             "timeout %d ssh -i %s -p 22 -o ConnectTimeout=%d %s@%s 'exit'",
             timeout, sshKeyPath, timeout, serverUser, serverIPv4);

    int result = system(sshCommand);

    // Check if SSH succeeded (exit status 0)
    if (WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        return 1; // Connection successful
    }
    return 0; // Connection failed or timed out
}

char* get_system_usage() {
    // Run the consolidated system usage script
    char* fullData = run_ssh_command(MAX_SYSTEM_USAGE_SIZE, "bash -c '%s/customScripts/systemUsage'", mcServerHome);

    if (!fullData) return strdup("Error fetching system data");

    // Allocate space for formatted system usage
    char* systemUsageStr = (char*)malloc(MAX_SYSTEM_USAGE_SIZE);
    if (!systemUsageStr) return NULL;

    // Format and copy the fetched data directly
    snprintf(systemUsageStr, MAX_SYSTEM_USAGE_SIZE, "%s", fullData);

    free(fullData);  // Clean up after usage
    return systemUsageStr;
}

int get_window_data() {
    windows[1].data = get_system_usage();  // System usage window
    if (!windows[1].data) return 0;
	return 1;
}

void ensure_script_on_server(const char* script_path, const char* remote_path_format, ...) {
    char remote_path[256];
    va_list args;

    va_start(args, remote_path_format);
    vsnprintf(remote_path, sizeof(remote_path), remote_path_format, args);
    va_end(args);

    char copy_cmd[512];
    snprintf(copy_cmd, sizeof(copy_cmd), 
             "scp -i %s -r %s %s@%s:%s", 
             sshKeyPath, script_path, serverUser, serverIPv4, remote_path);

    if (system(copy_cmd) == 0) {
        printf(" Srcipts copied successfully.\n");
    } else {
        fprintf(stderr, "Failed to copy script to server.\n");
    }
}

void* window_data_thread(void* arg) {
    int data_update_interval = 10; // Update once every 10 loops
    int counter = 0;

    while (running) {
            if (!get_window_data()) {
                exitError = 1;
                running = 0;
                return NULL;
            }
        napms(1000); // Short delay to prevent 100% CPU usage
    }
    return NULL;
}

void* log_data_thread(void* arg) {
	while (running) {
		windows[0].data = run_ssh_command(MAX_LOG_SIZE, "tail -n 2000 %s/logs/latest.log", mcServerHome); // Log window, 200 lines of history is probably enough
		if (autoScroll) scrollToBottom(0);
		napms(1000);
	}
}

int drawArray(int offsetX, int offsetY, char* array) {
	int arrayWidth = strlen(array);

	// Ensure arrayWidth is within bounds
	if (arrayWidth > maxTermX) {
		arrayWidth = maxTermX;
	}

	// Check for valid offset
	if (offsetX < 0 || offsetX >= maxTermX) {
		return 0;
	}
	if (offsetY < 0 || offsetY >= maxTermY) {
		return 0;
	}

	// Draw the array to the buffer
	for (int x = 0; x < arrayWidth; x++) {
		int bufferX = x + offsetX;
		if (bufferX >= maxTermX) {
			break; // Prevent out-of-bounds access
		}
		screenBuffer[offsetY][bufferX].character = array[x]; // Adjusted for proper offset handling
	}

	return 1;
}

void draw2dArray(int offsetX, int offsetY, char** array, int arrayHeight, int arrayWidth) {
    if (!screenBuffer || !array) return;

    for (int y = 0; y < arrayHeight; y++) {
        for (int x = 0; x < arrayWidth; x++) {
            if (array[y][x] != '\0') {
                screenBuffer[y + offsetY][x + offsetX].character = (uint_fast32_t)array[y][x];
                // Optionally set default control codes if needed
                strncpy(screenBuffer[y + offsetY][x + offsetX].controlFG, "default", sizeof(screenBuffer[y + offsetY][x + offsetX].controlFG) - 1);
                strncpy(screenBuffer[y + offsetY][x + offsetX].controlBG, "default", sizeof(screenBuffer[y + offsetY][x + offsetX].controlBG) - 1);
            }
        }
    }
}

void* render_thread(void* arg) {
    while (running) {
        getmaxyx(stdscr, maxTermY, maxTermX);
        if (!(maxTermX == prevMaxTermX && maxTermY == prevMaxTermY)) {
            resize_windows();
            getmaxyx(stdscr, prevMaxTermY, prevMaxTermX);
        }
		userInput();
		//generate_pipes_data();
        for (unsigned char i = 0; i < numWindows; i++) {
	        if (!draw_border_to_buffer(i)) return 0;
	    }
        update_screen();
        napms(33); // About 30 fps, good enough
    }
    return NULL;
}

// Each data thread can cause delay, so 
void start_threads() {
    pthread_t data_thread_id, render_thread_id, log_data_thread_id;
    pthread_create(&data_thread_id, NULL, window_data_thread, NULL);
    pthread_create(&render_thread_id, NULL, render_thread, NULL);
    pthread_create(&log_data_thread_id, NULL, log_data_thread, NULL);

    pthread_join(data_thread_id, NULL);
    pthread_join(render_thread_id, NULL);
    pthread_join(log_data_thread_id, NULL);
}

int main(int argc, char* argv[]) {
    // Initialize ncurses
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE); // Make getch() non-blocking
    getmaxyx(stdscr, maxTermY, maxTermX);
    
     // Initialize the screen buffer
    screenBuffer = (Character**)malloc(maxTermY * sizeof(Character*));
	Character emptyCharcater = {.character = ' ', .controlFG = "\\033[37", .controlBG = "\\033[30"};
	for (int i = 0; i < maxTermY; i++) {
	    screenBuffer[i] = (Character*)malloc(maxTermX * sizeof(Character));
	    for (int j = 0; j < maxTermX; j++) {
			screenBuffer[i][j] = emptyCharcater;
			// Fill with spaces, basically clearing the buffer so it's not just reading random memory data
	    }
	}
    
    // Handle all argvs
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
			serverIPv4 = argv[i + 1];
			i++; 
		} else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
			serverUser = argv[i + 1];
			i++; 
		} else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
			sshKeyPath = argv[i + 1];
			i++; 
		} else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
			mcServerHome = argv[i + 1];
			i++; 
		} else if (strcmp(argv[i], "-s") == 0) { // Silent, does not put something in the mc log
			silentMode = 1;
		} else {
			printf("Unknown or incomplete option: %s\n", argv[i]);
			return 1; // No mercy
		}
	}
	if (!(serverIPv4) || !(serverUser) || !(sshKeyPath)) {
		running = 0;
		printf("missing options");
		goto fatal;
	}

	if (!checkSSHConnection(30)) {
		drawArray(0, 0, "Initialization failed successfully (SSH Connection)\0");
		update_screen();
		napms(2500); // Delay so the message can actully be seen
		goto fatal;
	}
	
	ensure_script_on_server("./scripts/*", "%s/customScripts", mcServerHome);

    windows = (Window*)malloc(numWindows * sizeof(Window));

    create_window(3, 1, maxTermX/2 - 3, maxTermY - 2, "Logs");
    create_window(1 + maxTermX/2, 1, maxTermX/2 - 3, maxTermY/2 - 2, "System Usage");
    //create_window(1 + maxTermX/2, maxTermY/2, maxTermX/2 - 3, maxTermY/2 - 2, "Other");
	if (!silentMode) {
		FILE* localIPf = popen("./getOneLocalIP", "r");
		if (localIPf == NULL) {
	        perror("popen failed");
	        return 1;
	    }
	    // Read the output into the buffer
	    if (fgets(localIP, sizeof(localIP), localIPf) != NULL) {
	        // Remove the trailing newline if present
	        localIP[strcspn(localIP, "\n")] = '\0';
	        printf("IP Address: %s\n", localIP);
	    } else {
	        printf("No IP found\n");
	    }
	    drawArray(0, 4, "Minecraft system log notified! :O\0"); // Write to the buffer, it'll be displayed later
	}
	
    if (!silentMode) {
		run_ssh_command(1024, "echo '[$(date +\"%%H:%%M:%%S\")] $(hostname) started the custom monitor.' >> %s/logs/latest.log", mcServerHome); // 1024 even though it does not get the return
	}
	napms(500);
	drawArray(0, 0, "Initialization started successfully\0");
	drawArray(0, 2, "I recommend using terminal size 150x70 or higher.\0");
	drawArray(0, 3, "Make sure you have at least 50MiB of ram free :3\0"); // Bad apple is INTENDSE
		
	update_screen();
	napms(5000); // Delay so the message can actully be seen
	
	update_screen();
	
    start_threads();
    
	fatal:
    // Clean up
    for (int i = 0; i < maxTermY; i++) {
        free(screenBuffer[i]);
    }
    free(screenBuffer);
    clear();
    free(windows);
    endwin();
    return 0;
}

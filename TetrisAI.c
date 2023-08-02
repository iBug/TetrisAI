#include <conio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

const char *const INFO_AUTHOR = "iBug";
const char *const INFO_VERSION = "1.10 c";

// Blocks
enum {
	TETRIS_I = 0,
	TETRIS_T,
	TETRIS_L,
	TETRIS_J,
	TETRIS_Z,
	TETRIS_S,
	TETRIS_O
};

// =============================================================================
// Rotation states
static const uint16_t gs_uTetrisTable[7][4] = {
	{0x00F0U, 0x2222U, 0x00F0U, 0x2222U}, // I
	{0x0072U, 0x0262U, 0x0270U, 0x0232U}, // T
	{0x0223U, 0x0074U, 0x0622U, 0x0170U}, // L
	{0x0226U, 0x0470U, 0x0322U, 0x0071U}, // J
	{0x0063U, 0x0264U, 0x0063U, 0x0264U}, // Z
	{0x006CU, 0x0462U, 0x006CU, 0x0462U}, // S
	{0x0660U, 0x0660U, 0x0660U, 0x0660U}  // O
};

// =============================================================================
// Initial pool state
// From 0 (top row) to 28 (bottom row)
// More 1's for collision detection
// Pool width = 16 - 2 * 2 = 12
// 0xFFFFU = Full row
static const uint16_t gs_uInitialTetrisPool[28] = {
	0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U,
	0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U,
	0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U,
	0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xC003U, 0xFFFFU, 0xFFFFU
};

#define COL_BEGIN 2
#define COL_END 14
#define ROW_BEGIN 4
#define ROW_END 26

// =============================================================================
typedef struct TetrisManager {
	uint16_t pool[28];
	int8_t x;
	int8_t y;
	int8_t type[3];
	int8_t orientation[3];
} TetrisManager;

// =============================================================================
typedef struct TetrisControl {
	int8_t color[28][16];
	bool dead;
	bool pause;
	bool clockwise;
	int8_t direction;
	bool model;
	uint16_t frameRate;
	unsigned score;
	unsigned erasedCount[4]; // How many rows are erased at once
	unsigned erasedTotal;
	unsigned tetrisCount[7];
	unsigned tetrisTotal;
} TetrisControl;

HANDLE hConsoleOutput;
struct {
	int8_t model;
	int8_t benchmark;
	double startTime;
	bool resetFpsCounter;
} general;

unsigned long fpsRate = 800;
unsigned short lastInsertedHeight = 0;
// =============================================================================
// Easter Eggs:
// [0]
// [1]
// [2] Turbo Boost
// [3]
#define easterNum 4
int8_t easterEgg[easterNum] = {0};

// =============================================================================
// Function board
bool isWindowsTerminal();

void autoRun(TetrisManager *manager, TetrisControl *control);
void benchmark(TetrisManager *manager, TetrisControl *control);
signed long benchmarkRun(TetrisManager *manager, TetrisControl *control);
signed long calcFPS(void);
bool checkCollision(const TetrisManager *manager);
bool checkErasing(TetrisManager *manager, TetrisControl *control);
void clrscr(void);
double getTime(void);
void dropDownTetris(TetrisManager *manager, TetrisControl *control);
bool enableDebugPrivilege();
void flushPrint();
void giveTetris(TetrisManager *manager, TetrisControl *control);
void gotoxyInPool(short x, short y);
void gotoxyWithFullwidth(short x, short y);
bool horzMoveTetris(TetrisManager *manager, TetrisControl *control);
void initGame(TetrisManager *manager, TetrisControl *control, bool model);
void insertTetris(TetrisManager *manager);
bool keydownControl(TetrisManager *manager, TetrisControl *control, int key);
int mainMenu(void);
bool moveDownTetris(TetrisManager *manager, TetrisControl *control);
void pause(void);
void printCurrentTetris(const TetrisManager *manager,
						const TetrisControl *control);
void printNextTetris(const TetrisManager *manager);
void printPoolBorder();
void printPrompting(const TetrisControl *control);
void printScore(const TetrisManager *manager, const TetrisControl *control);
void printTetrisPool(const TetrisManager *manager,
					 const TetrisControl *control);
void removeTetris(TetrisManager *manager);
bool rotateTetris(TetrisManager *manager, TetrisControl *control);
void runGame(TetrisManager *manager, TetrisControl *control);
void setPoolColor(const TetrisManager *manager, TetrisControl *control);

// =============================================================================
// iBug's C-Sync technology!
CHAR_INFO outputBuffer[25][80];
const SMALL_RECT outputRegion = {0, 0, 79, 24};
const COORD outputBufferSize = {80, 25}, zeroPosition = {0, 0};
COORD outputCursorPosition = {0, 0};
WORD outputAttribute = 0x07;
void IncrementOutputCursorPosition();

char charBuf[128];
#define buffer_printf(args...)\
	{\
		snprintf(charBuf, sizeof(charBuf), args);\
		buffer_print(charBuf);\
	}
void buffer_SetConsoleCursorPosition(HANDLE, COORD Pos);
void buffer_SetConsoleTextAttribute(HANDLE, WORD Attribute);
void buffer_print(LPCSTR String);

// =============================================================================
int main(int argc, char *argv[]) {
	// Restart with conhost.exe if running in WT.
	if (IsWindowsTerminal()) {
		char currentPath[MAX_PATH];
		GetModuleFileNameA(NULL, currentPath, MAX_PATH);
	
		char command[MAX_PATH + 15];
		sprintf_s(command, "conhost.exe %s", currentPath);
		system(command);
		return 0;
	}
	// In case there's any cmdline arguments
	if (argc > 1) {
		char optionEx[128];
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '/' || argv[i][0] == '-') {
				argv[i]++;
				// Convert to lowercase
				char *a = argv[i];
				while (*a != '\0' && *a != ':') {
					if (*a >= 'A' && *a <= 'Z') {
						*a += 0x20;
					}
					a++;
				}
				if (*a == ':') {
					*a = '\0';
					a++;
					strcpy(optionEx, a);
				} else {
					memset(optionEx, 0, sizeof(optionEx));
				}

				if (strcmp(argv[i], "c16") == 0) {
					easterEgg[1] = 1;
				} else if (strcmp(argv[i], "boost") == 0) {
					easterEgg[2] = 1;
				} else if (strcmp(argv[i], "fpsrate") == 0) {
					long newfps = atol(optionEx);
					if (newfps > 0) {
						fpsRate = newfps;
						if (fpsRate > 5000) {
							fpsRate = 5000;
						}
						easterEgg[3] = 1;
					}
				} else if (strcmp(argv[i], "turbo") == 0) {
					enableDebugPrivilege();
					HANDLE thisproc = GetCurrentProcess();
					HANDLE thisthread = GetCurrentThread();
					int procprio, threadprio;
					procprio = REALTIME_PRIORITY_CLASS;
					threadprio = THREAD_PRIORITY_TIME_CRITICAL;
					SetPriorityClass(thisproc, procprio);
					SetThreadPriority(thisthread, threadprio);
				} else {
					printf("Error: Unknown command line option \"%s\"\n", argv[i]);
					return 1;
				}
			}
		}
	}

	// Initialize Console
	TetrisManager tetrisManager;
	TetrisControl tetrisControl;
	hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO cursorInfo = {1, FALSE};
	CONSOLE_SCREEN_BUFFER_INFO screenInfo;

	GetConsoleScreenBufferInfo(hConsoleOutput, &screenInfo);
	COORD bufferSize;
	/*
	bufferSize.X = 1 + screenInfo.srWindow.Right;
	bufferSize.Y = 1 + screenInfo.srWindow.Bottom;
	*/
	bufferSize.X = 80;
	bufferSize.Y = 25;

	SMALL_RECT consoleWindowPos = {0, 0, 79, 24};
	SetConsoleWindowInfo(hConsoleOutput, TRUE, &consoleWindowPos);
	SetConsoleScreenBufferSize(hConsoleOutput, bufferSize);
	clrscr();

	SetConsoleCursorInfo(hConsoleOutput, &cursorInfo);
	snprintf(charBuf, sizeof(charBuf), "Tetris with AI ver %s", INFO_VERSION);
	SetConsoleTitleA(charBuf);

	DeleteMenu(GetSystemMenu(GetConsoleWindow(), 0), SC_MAXIMIZE, MF_DISABLED);

	do {
		general.model = 0;
		general.model = mainMenu();
		if (general.model != 2) {
			general.benchmark = 0;
		} else {
			general.benchmark = 1;
		}
		buffer_SetConsoleTextAttribute(hConsoleOutput, 0x07);
		clrscr();
		initGame(&tetrisManager, &tetrisControl, general.model == 0);
		printPrompting(&tetrisControl);
		printPoolBorder();

		int8_t prevEE2;
		switch (general.model) {
			case 0:
				tetrisControl.frameRate = 450;
				runGame(&tetrisManager, &tetrisControl);
				break;
			case 1:
				autoRun(&tetrisManager, &tetrisControl);
				break;
			case 2:
				prevEE2 = easterEgg[2];
				easterEgg[2] = 1;

				buffer_SetConsoleTextAttribute(hConsoleOutput, 0x07);
				clrscr();
				initGame(&tetrisManager, &tetrisControl, 0);
				printPrompting(&tetrisControl);
				printPoolBorder();
				signed long mark = benchmarkRun(&tetrisManager, &tetrisControl);

				clrscr();
				buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0F);
				gotoxyWithFullwidth(14, 5);
				buffer_print("┏━━━━━━━━━━┓");
				gotoxyWithFullwidth(14, 6);
				buffer_print("┃   Tetris with AI   ┃");
				gotoxyWithFullwidth(14, 7);
				buffer_print("┃   Benchmark Mode   ┃");
				gotoxyWithFullwidth(14, 8);
				buffer_print("┗━━━━━━━━━━┛");
				gotoxyWithFullwidth(14, 9);
				buffer_printf("Author: %s", INFO_AUTHOR);
				gotoxyWithFullwidth(15, 11);
				buffer_printf("Score: %6ld", mark);

				flushPrint();
				pause();
				easterEgg[2] = prevEE2;
				continue;
				break;
			default:
				return -1;
		}

		buffer_SetConsoleTextAttribute(hConsoleOutput, 0xF0);
		gotoxyWithFullwidth(12, 9);
		buffer_print("                    ");
		gotoxyWithFullwidth(12, 10);
		buffer_print(easterEgg[1] ? "   Stack Overflow   " : "   Press any key.   ");
		gotoxyWithFullwidth(12, 11);
		buffer_print("                    ");
		buffer_SetConsoleTextAttribute(hConsoleOutput, 0x07);
		flushPrint();
		pause();
		clrscr();

	} while (1);

	gotoxyWithFullwidth(0, 0);
	// CloseHandle(hConsoleOutput);
	return 0;
}

// =============================================================================
BOOL IsWindowsTerminal()
{
    HWND hwndConsole = GetForegroundWindow();

    if (hwndConsole != NULL)
    {
        DWORD dwConsoleProcessId;
        GetWindowThreadProcessId(hwndConsole, &dwConsoleProcessId);

        HANDLE hConsoleProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwConsoleProcessId);
        if (hConsoleProcess != NULL)
        {
            WCHAR processPath[MAX_PATH];
            DWORD dwSize = MAX_PATH;

            if (QueryFullProcessImageNameW(hConsoleProcess, 0, processPath, &dwSize) != 0)
            {
                WCHAR* fileName = wcsrchr(processPath, L'\\');
                if (fileName != NULL)
                {
                    std::wcout<<fileName;
                	if (_wcsicmp(fileName + 1, L"WindowsTerminal.exe") == 0)
                    {
                        CloseHandle(hConsoleProcess);
                        return TRUE;
                    }
                }
            }

            CloseHandle(hConsoleProcess);
        }
    }

    return FALSE;
}

// =============================================================================
void initGame(TetrisManager *manager, TetrisControl *control, bool model) {
	memset(manager, 0, sizeof(TetrisManager));

	memcpy(manager->pool, gs_uInitialTetrisPool, sizeof(uint16_t[28]));
	if (general.benchmark) {
		srand(2014530982);
	} else {
		srand((unsigned)time(NULL));
	}

	// Next and the next after next
	manager->type[1] = rand() % 7;
	manager->orientation[1] = rand() & 3;
	manager->type[2] = rand() % 7;
	manager->orientation[2] = rand() & 3;
	if (general.benchmark) {
		manager->type[1] = 0;
		manager->type[2] = 1;
	}

	memset(control, 0, sizeof(TetrisControl));
	control->model = model;

	giveTetris(manager, control);
	setPoolColor(manager, control);
	printScore(manager, control);
	printTetrisPool(manager, control);
	general.startTime = getTime();
	general.resetFpsCounter = true;
}

// =============================================================================
void giveTetris(TetrisManager *manager, TetrisControl *control) {
	static uint16_t tetris;
	static uint16_t num;

	manager->type[0] = manager->type[1];
	manager->orientation[0] = manager->orientation[1];

	manager->type[1] = manager->type[2];
	manager->orientation[1] = manager->orientation[2];

	num = rand();
	/*
	if (general.model==1)
	{
	        if (lastInsertedHeight<=16 && num%7==0)
	        {
	                manager->type[2] = 1+num%6;
	        }
	        else if (num >= 40000-lastInsertedHeight*300)
	        {
	                manager->type[2] = 0;
	        }
	        else
	        {
	                manager->type[2] = num%7;
	        }
	}
	*/
	if (num >= 32500) {
		manager->type[2] = 0;
	} else {
		manager->type[2] = num % 7;
	}
	if (general.benchmark) {
		manager->type[2] = (manager->type[1] + 1) % 7;
	}
	manager->orientation[2] = rand() & 3;

	tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

	if (tetris & 0xF000) {
		manager->y = 0;
	} else {
		manager->y = (tetris & 0xFF00) ? 1 : 2;
	}
	manager->x = 6;

	if (checkCollision(manager)) {
		control->dead = true;
	} else {
		insertTetris(manager);
	}

	++control->tetrisTotal;
	++control->tetrisCount[manager->type[0]];

	printNextTetris(manager);
}

// =============================================================================
bool checkCollision(const TetrisManager *manager) {
	uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];
	uint16_t dest = 0U;

	dest |= (((manager->pool[manager->y + 0] >> manager->x) << 0x0) & 0x000F);
	dest |= (((manager->pool[manager->y + 1] >> manager->x) << 0x4) & 0x00F0);
	dest |= (((manager->pool[manager->y + 2] >> manager->x) << 0x8) & 0x0F00);
	dest |= (((manager->pool[manager->y + 3] >> manager->x) << 0xC) & 0xF000);

	return ((dest & tetris) != 0);
}

// =============================================================================
void insertTetris(TetrisManager *manager) {
	uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

	manager->pool[manager->y + 0] |= (((tetris >> 0x0) & 0x000F) << manager->x);
	manager->pool[manager->y + 1] |= (((tetris >> 0x4) & 0x000F) << manager->x);
	manager->pool[manager->y + 2] |= (((tetris >> 0x8) & 0x000F) << manager->x);
	manager->pool[manager->y + 3] |= (((tetris >> 0xC) & 0x000F) << manager->x);
}

// =============================================================================
void removeTetris(TetrisManager *manager) {
	uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

	manager->pool[manager->y + 0] &= ~(((tetris >> 0x0) & 0x000F) << manager->x);
	manager->pool[manager->y + 1] &= ~(((tetris >> 0x4) & 0x000F) << manager->x);
	manager->pool[manager->y + 2] &= ~(((tetris >> 0x8) & 0x000F) << manager->x);
	manager->pool[manager->y + 3] &= ~(((tetris >> 0xC) & 0x000F) << manager->x);
}

// =============================================================================
void setPoolColor(const TetrisManager *manager, TetrisControl *control) {

	int8_t i, x, y;

	uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

	for (i = 0; i < 16; ++i) {
		y = (i >> 2) + manager->y;
		if (y > ROW_END) {
			break;
		}
		x = (i & 3) + manager->x;
		if ((tetris >> i) & 1) {
			control->color[y][x] = (manager->type[0] | 8);
		}
	}
}

// =============================================================================
bool rotateTetris(TetrisManager *manager, TetrisControl *control) {
	int8_t ori = manager->orientation[0];

	removeTetris(manager);

	manager->orientation[0] =
		(control->clockwise) ? ((ori + 1) & 3) : ((ori + 3) & 3);

	if (checkCollision(manager)) {
		manager->orientation[0] = ori;
		insertTetris(manager);
		return false;
	} else {
		insertTetris(manager);
		setPoolColor(manager, control);
		if (!easterEgg[2] || control->model == 0) {
			printCurrentTetris(manager, control);
		}
		return true;
	}
}

// =============================================================================
bool horzMoveTetris(TetrisManager *manager, TetrisControl *control) {
	int x = manager->x;

	removeTetris(manager);
	control->direction == 0 ? (--manager->x) : (++manager->x);

	if (checkCollision(manager)) {
		manager->x = x;
		insertTetris(manager);
		return false;
	} else {
		insertTetris(manager);
		setPoolColor(manager, control);
		if (!easterEgg[2] || !control->model) {
			printCurrentTetris(manager, control);
		}
		return true;
	}
}

// =============================================================================
bool moveDownTetris(TetrisManager *manager, TetrisControl *control) {
	int8_t y = manager->y;

	removeTetris(manager);
	++manager->y;

	if (checkCollision(manager)) {
		manager->y = y;
		insertTetris(manager);
		if (checkErasing(manager, control)) {
			if (control->frameRate > 0) {
				Sleep(2 * control->frameRate);
			}
			printTetrisPool(manager, control);
		}
		return false;
	} else {
		insertTetris(manager);
		setPoolColor(manager, control);
		printCurrentTetris(manager, control);
		return true;
	}
}

// =============================================================================
void dropDownTetris(TetrisManager *manager, TetrisControl *control) {
	removeTetris(manager);

	for (; manager->y < ROW_END; ++manager->y) {
		if (checkCollision(manager)) {
			break;
		}
	}
	lastInsertedHeight = --manager->y;

	insertTetris(manager);
	setPoolColor(manager, control);

	printTetrisPool(manager, control);
	if (checkErasing(manager, control) && control->frameRate > 0) {
		Sleep(2 * control->frameRate);
		printTetrisPool(manager, control);
	}
}

// =============================================================================
bool checkErasing(TetrisManager *manager, TetrisControl *control) {
	static const unsigned scores[5] = {0, 10, 30, 90, 150};
	static const unsigned scoreRatios[5] = {0, 1, 5, 15, 25};
	int8_t lowest = 0;
	int8_t count = 0;
	int8_t k = 0, y = manager->y + 3;

	do {
		if (y < ROW_END && manager->pool[y] == 0xFFFFU) {
			count++;

			lowest = ROW_END - y;

			memmove(manager->pool + 1, manager->pool, sizeof(uint16_t) * y);

			memmove(control->color[1], control->color[0], sizeof(int8_t[16]) * y);
		} else {
			--y;
			++k;
		}
	} while (y >= manager->y && k < 4);

	lowest--;
	control->erasedTotal += count;
	control->score += scores[count] + lowest * scoreRatios[count] + 1;

	if (count > 0) {
		++control->erasedCount[count - 1];
	}

	giveTetris(manager, control);
	setPoolColor(manager, control);
	printScore(manager, control);

	return (count > 0);
}

// =============================================================================
bool keydownControl(TetrisManager *manager, TetrisControl *control, int key) {
	static signed char lastAction;
	static bool ret = false;

	if (general.model == 0 && lastAction >= 0) {
		gotoxyWithFullwidth(27, 10 + lastAction);
		buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0B);
		buffer_print("□");
	}

	if (key == 13) { // Pause/Unpause
		lastAction = 6;

		control->pause = !control->pause;

		gotoxyWithFullwidth(27, 10 + lastAction);
		buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0B);
		if (control->pause) {
			buffer_print("■");
		} else {
			buffer_print("□");
		}
	}

	if (control->pause) {
		return false;
	}

	switch (key) {
		case 'w':
		case 'W':
		case 72:
			lastAction = 3;
			control->clockwise = true;
			ret = rotateTetris(manager, control);
			break;
		case 'a':
		case 'A':
		case 75:
			lastAction = 0;
			control->direction = 0;
			ret = horzMoveTetris(manager, control);
			break;
		case 'd':
		case 'D':
		case 77:
			lastAction = 1;
			control->direction = 1;
			ret = horzMoveTetris(manager, control);
			break;
		case 's':
		case 'S':
		case 80:
			lastAction = 2;
			ret = moveDownTetris(manager, control);
			break;
		case ' ':
			lastAction = 5;
			dropDownTetris(manager, control);
			ret = true;
			break;
		case 'x':
		case 'X':
		case '0':
			lastAction = 4;
			control->clockwise = false;
			ret = rotateTetris(manager, control);
			break;
		default:
			lastAction = -1;
			break;
	}

	if (general.model == 0 && lastAction >= 0) {
		gotoxyWithFullwidth(27, 10 + lastAction);
		buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0B);
		buffer_print("■");
	}
	return ret;
}

// =============================================================================
inline void gotoxyWithFullwidth(short x, short y) {
	static COORD cd;

	cd.X = (short)(x << 1);
	cd.Y = y;
	buffer_SetConsoleCursorPosition(hConsoleOutput, cd);
}

// =============================================================================
int mainMenu() {
	static int indexTotal = 3;
	if (easterEgg[3]) {
		indexTotal = 2; // Hide benchmark mode under debugging mode
	}
	static const char *const modelItem[] = {
		"1. Play Now", "2. iBug's Marvel", "3. Benchmark mode"
	};
#define secretNum 3
	static const char *const secretCode[secretNum] = {"xzsyw", "sososo", "boost"};
	static char secretKey[1024];
	signed long int index = 0, secretIndex = 0, ch;

	buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0F);
	gotoxyWithFullwidth(14, 5);
	buffer_print("┏━━━━━━━━━━┓");
	gotoxyWithFullwidth(14, 6);
	buffer_print("┃   Tetris with AI   ┃");
	gotoxyWithFullwidth(14, 7);
	buffer_printf("┃   Version: %s", INFO_VERSION);
	gotoxyWithFullwidth(25, 7);
	buffer_print("┃");
	gotoxyWithFullwidth(14, 8);
	buffer_print("┗━━━━━━━━━━┛");
	gotoxyWithFullwidth(14, 9);
	buffer_printf("Author: %s", INFO_AUTHOR);

	buffer_SetConsoleTextAttribute(hConsoleOutput, 0xF0);
	for (int i = 0; i < indexTotal; i++) {
		gotoxyWithFullwidth(14, 14 + 2 * i);
		buffer_printf("%2s%s%2s", "", modelItem[i], "");
		buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0F);
	}

	do {
		flushPrint();
		ch = _getch();
		switch (ch) {
			/*
			// For cheat keys
			case 'w': case 'W': case '8': case 72:  // Up
			case 'a': case 'A': case '4': case 75:  // Left
			case 'd': case 'D': case '6': case 77:  // Right
			case 's': case 'S': case '2': case 80:  // Down
			*/

			//===========================================
			case 72:
			case 75: // U/L
			case 77:
			case 80: // D/R
				buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0F);
				gotoxyWithFullwidth(14, 14 + 2 * index);
				buffer_printf("%2s%s%2s", "", modelItem[index], "");
				if (ch == 72 || ch == 75) {
					index--;
					if (index < 0) {
						index += indexTotal;
					}
				}
				if (ch == 77 || ch == 80) {
					index++;
					if (index >= indexTotal) {
						index -= indexTotal;
					}
				}
				buffer_SetConsoleTextAttribute(hConsoleOutput, 0xF0);
				gotoxyWithFullwidth(14, 14 + 2 * index);
				buffer_printf("%2s%s%2s", "", modelItem[index], "");
				flushPrint();
				break;
			case ' ':
			case 13: // Space/Enter
				return index;
			case 27: // Esc
				exit(0);
				return -1;
			case 8: // Backspace
				memset(secretKey, 0, sizeof(secretKey));
				secretIndex = 0;
				break;
			default:
				if (ch >= 'A' && ch <= 'Z') {
					ch += 0x20;
				}
				if (ch < 'a' || ch > 'z') {
					break;
				}
				secretKey[secretIndex] = ch;
				secretIndex++;
				for (int i = 0; i < secretNum; i++) {
					if (strcmp(secretKey, secretCode[i]) == 0) {
						easterEgg[i] = !easterEgg[i];
						memset(secretKey, 0, sizeof(secretKey));
						secretIndex = 0;
					}
				}
				break;
		}
	} while (1);
}

// =============================================================================
void printPoolBorder() {
	int8_t y;

	buffer_SetConsoleTextAttribute(hConsoleOutput, 0xF0);
	for (y = ROW_BEGIN; y < ROW_END; ++y) {
		gotoxyWithFullwidth(10, y - 3);
		buffer_print("  ");
		gotoxyWithFullwidth(23, y - 3);
		buffer_print("  ");
	}

	gotoxyWithFullwidth(10, y - 3);
	buffer_print("                            ");
	flushPrint();
}

inline void gotoxyInPool(short x, short y) {
	gotoxyWithFullwidth(x + 9, y - 3);
}

// =============================================================================
// iBug's "Sync" implementation
static uint16_t ppool[16][28] = {{0}};

void printTetrisPool(const TetrisManager *manager,
					 const TetrisControl *control) {
	int8_t x, y;
	printNextTetris(manager);

	for (y = ROW_BEGIN; y < ROW_END; ++y) {
		for (x = COL_BEGIN; x < COL_END; ++x) {
			if ((manager->pool[y] >> x) & 1) {
				if (control->color[y][x] == ppool[x][y])
					continue;
				gotoxyInPool(x, y);
				buffer_SetConsoleTextAttribute(hConsoleOutput, control->color[y][x]);
				buffer_print("■");
				ppool[x][y] = control->color[y][x];
			} else {
				if (ppool[x][y] == 0)
					continue;
				// SetConsoleTextAttribute(hConsoleOutput, 0);
				gotoxyInPool(x, y);
				buffer_print("  ");
				ppool[x][y] = 0;
			}
		}
	}
	flushPrint();
}

// =============================================================================
void printCurrentTetris(const TetrisManager *manager,
						const TetrisControl *control) {
	int8_t x, y;

	y = (manager->y > ROW_BEGIN) ? (manager->y - 1) : ROW_BEGIN;
	for (; y < ROW_END && y < manager->y + 4; ++y) {
		x = (manager->x > COL_BEGIN) ? (manager->x - 1) : COL_BEGIN;
		for (; x < COL_END && x < manager->x + 5; ++x) {
			gotoxyInPool(x, y);
			if ((manager->pool[y] >> x) & 1) {
				buffer_SetConsoleTextAttribute(hConsoleOutput, control->color[y][x]);
				buffer_print("■");
				ppool[x][y] = control->color[y][x];

			} else {
				if (ppool[x][y] == 0)
					continue;
				// SetConsoleTextAttribute(hConsoleOutput, 0);
				buffer_print("  ");
				ppool[x][y] = 0;
			}
		}
	}
	flushPrint();
}

// =============================================================================
void printNextTetris(const TetrisManager *manager) {
	int8_t i;
	uint16_t tetris;

	tetris = gs_uTetrisTable[manager->type[1]][manager->orientation[1]];
	buffer_SetConsoleTextAttribute(hConsoleOutput, manager->type[1] | 8);
	for (i = 0; i < 16; ++i) {
		gotoxyWithFullwidth((i & 3) + 27, (i >> 2) + 2);
		((tetris >> i) & 1) ? buffer_print("■") : buffer_print("  ");
	}

	if (general.benchmark) {
		// Don't show the second one under benchmark mode
		return;
	} else {
		// The second one shouldn't have color
		tetris = gs_uTetrisTable[manager->type[2]][manager->orientation[2]];
		buffer_SetConsoleTextAttribute(hConsoleOutput, 8);
		for (i = 0; i < 16; ++i) {
			gotoxyWithFullwidth((i & 3) + 32, (i >> 2) + 2);
			((tetris >> i) & 1) ? buffer_print("■") : buffer_print("  ");
		}
	}
}

// =============================================================================
void printScore(const TetrisManager *manager, const TetrisControl *control) {
	int8_t i;

	buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0E);

	gotoxyWithFullwidth(5, 6);
	buffer_printf("%u", control->score);

	for (i = 0; i < 4; ++i) {
		gotoxyWithFullwidth(6, 10 + i);
		buffer_printf("%u", control->erasedCount[i]);
	}
	gotoxyWithFullwidth(6, 8);
	buffer_printf("%u", control->erasedTotal);

	for (i = 0; i < 7; ++i) {
		gotoxyWithFullwidth(6, 17 + i);
		buffer_printf("%u", control->tetrisCount[i]);
	}
	gotoxyWithFullwidth(6, 15);
	buffer_printf("%u", control->tetrisTotal);
}

// =============================================================================
void printPrompting(const TetrisControl *control) {
	static const char *const modelName[] = {"Play Now",
											"iBug's Marvel"
										   };
	static const char *const easterEggName = "Master's Spell";
	static const char *const tetrisName = "ITLJZSO";
	buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0E);

	gotoxyWithFullwidth(1, 1);
	if (general.benchmark) {
		buffer_print("■Benchmark Mode");
	} else {
		if (easterEgg[0]) {
			buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0C);
			buffer_print("■%s");
			buffer_print(easterEggName);
			buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0E);
		}
		buffer_print("■");
		buffer_print(control->model ? modelName[0] : modelName[1]);
	}
	gotoxyWithFullwidth(1, 3);
	buffer_print("□[Esc] Exit");

	gotoxyWithFullwidth(1, 6);
	buffer_printf("■Score %u", control->score);
	gotoxyWithFullwidth(1, 8);
	buffer_printf("■Erased: %u", control->erasedTotal);

	int8_t i;
	for (i = 0; i < 4; ++i) {
		gotoxyWithFullwidth(2, 10 + i);
		buffer_printf("□%dL:   %u", i + 1, control->erasedCount[i]);
	}
	gotoxyWithFullwidth(1, 15);
	buffer_printf("■Blocks: %u", control->tetrisTotal);

	for (i = 0; i < 7; ++i) {
		gotoxyWithFullwidth(2, 17 + i);
		buffer_printf("□%c:    %u", tetrisName[i], control->tetrisCount[i]);
	}

	buffer_SetConsoleTextAttribute(hConsoleOutput, 0xF);
	if (general.benchmark) {
		// Don't show the second one under benchmark mode
		gotoxyWithFullwidth(26, 1);
		buffer_print("┏━━━━┓");
		gotoxyWithFullwidth(26, 2);
		buffer_print("┃        ┃");
		gotoxyWithFullwidth(26, 3);
		buffer_print("┃        ┃");
		gotoxyWithFullwidth(26, 4);
		buffer_print("┃        ┃");
		gotoxyWithFullwidth(26, 5);
		buffer_print("┃        ┃");
		gotoxyWithFullwidth(26, 6);
		buffer_print("┗━━━━┛");
	} else {
		gotoxyWithFullwidth(26, 1);
		buffer_print("┏━━━━┳━━━━┓");
		gotoxyWithFullwidth(26, 2);
		buffer_print("┃        ┃        ┃");
		gotoxyWithFullwidth(26, 3);
		buffer_print("┃        ┃        ┃");
		gotoxyWithFullwidth(26, 4);
		buffer_print("┃        ┃        ┃");
		gotoxyWithFullwidth(26, 5);
		buffer_print("┃        ┃        ┃");
		gotoxyWithFullwidth(26, 6);
		buffer_print("┗━━━━┻━━━━┛");
	}

	buffer_SetConsoleTextAttribute(hConsoleOutput, 0xB);
	if (!control->model) { // Auto mode

		if (general.benchmark) {
			gotoxyWithFullwidth(26, 8);
			buffer_print("■Benchmark");
			gotoxyWithFullwidth(27, 10);
			buffer_print("□[Esc]: Exit");

			buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0B);
			gotoxyWithFullwidth(26, 20);
			buffer_printf("■Current Score: %6u", 100 * (unsigned)0.0);
			gotoxyWithFullwidth(26, 21);
			buffer_printf("■Computer Rank: %6u", 100 * (unsigned)0.0);
		} else {
			gotoxyWithFullwidth(26, 8);
			buffer_print("■Watch Marvel");
			gotoxyWithFullwidth(27, 10);
			buffer_print("□Speed up:  ↑ W +");
			gotoxyWithFullwidth(27, 11);
			buffer_print("□Slow down: ↓ S -");
			gotoxyWithFullwidth(27, 12);
			buffer_print("□Pause:     Space Enter");
			gotoxyWithFullwidth(27, 13);
			buffer_print("□Exit:      Esc");

			gotoxyWithFullwidth(26, 21);
			buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0B);
			buffer_printf("■%6.2lf fps", 0.0);
		}
	} else {
		gotoxyWithFullwidth(26, 8);
		buffer_print("■Manual run");
		gotoxyWithFullwidth(27, 10);
		buffer_print("□Move left:  ← A 4");
		gotoxyWithFullwidth(27, 11);
		buffer_print("□Move right: → D 6");
		gotoxyWithFullwidth(27, 12);
		buffer_print("□Move down:  ↓ S 2");
		gotoxyWithFullwidth(27, 13);
		buffer_print("□Clockwise:  ↑ W 8");
		gotoxyWithFullwidth(27, 14);
		buffer_print("□CounterCW:     X 0");
		gotoxyWithFullwidth(27, 15);
		buffer_print("□Drop down:  Space");
		gotoxyWithFullwidth(27, 16);
		buffer_print("□Pause:      Enter");
		gotoxyWithFullwidth(27, 17);
		buffer_print("□End game:   Esc");
	}

	gotoxyWithFullwidth(25, 23);
	buffer_printf("■By: %s", INFO_AUTHOR);
	flushPrint();
}

// =============================================================================
void runGame(TetrisManager *manager, TetrisControl *control) {
	int ch;
	static double clockLast, clockNow;
	control->model = 0;

	clockLast = getTime();
	printTetrisPool(manager, control);

	while (!control->dead) {
		while (_kbhit()) {
			ch = _getch();
			if (ch == 27) {
				return;
			}
			keydownControl(manager, control, ch);
		}

		if (!control->pause) {
			clockNow = getTime();
			if (clockNow - clockLast > control->frameRate) {
				clockLast = clockNow;
				moveDownTetris(manager, control);
			}
		}
	}
}

/*******************************************************************************
以下内容中思想来自网络，算法及代码为Mr. iBug原创

改进的法：（只考虑当前方块）

非小学生可跳过此段内容

一、尝试着对当前落子的每一种旋转变换、从左到右地摆放，产生所有摆法。

二、对每一种摆法进行评价。评价包含如下6项指标：

    1.下落高度（Landing Height）：
        当前方块落下去之后，方块中点距底部的方格数
        事实上，不求中点也是可以的，详见网址

    2.消行数（Rows Eliminated）
        消行层数与当前方块贡献出的方格数乘积

    3.行变换（Row Transitions）：
        从左到右（或者反过来）检测一行，当该行中某个方格从有方块到无方块（或无方块到有方块），
        视为一次变换。游戏池边界算作有方块。行变换从一定程度上反映出一行的平整程度，越平整值越小
        该指标为所有行的变换数之和
        如图：■表示有方块，□表示空格（游戏池边界未画出）
        ■■□□■■□□■■□□ 变换数为6
        □□□□□■□■□■□■ 变换数为8
        ■■■■□□□□□□■■ 变换数为2
        ■■■■■■■■■■■■ 变换数为0

    4.列变换（Column Transitions）：大意同上
        列变换从一定程度上反映出一列中空洞的集中程度，空洞越集中值越小

    5.空洞数（Number of Holes）
        不解释

    6.井的总和（Well Sums）：
        井指两边皆有方块的空列。该指标为所有井的深度连加到1再求总和
        注意一列中可能有多个井，如图：
        ■□□
        ■□■
        ■□■
        ■■■
        ■□■
        ■□■
        ■□■
        中间一列为井，深度连加到一的和为 (2+1)+(3+2+1)=9

    各项指标权重经验值：
    1	-4.500158825082766
    2	3.4181268101392694
    3	-3.2178882868487753
    4	-9.348695305445199
    5	-7.899265427351652
    6	-3.3855972247263626

三、比较每一种摆法的评分，取最高者。当评分相同时，比较优先度
    计算公式

        落于左侧的摆法：100 * 水平平移格子数 + 10 + 旋转次数;

        落于右侧的摆法：100 * 水平平移格子数 + 旋转次数;
*******************************************************************************/

typedef struct _AIPlacing {
	uint16_t action;
	uint16_t priority;
	double value;
} AIPlacing;

void putDownTetris(TetrisManager *manager);
int calcLanding(TetrisManager *manager);
int calcTrans(const TetrisManager *manager);
int calcStatus(const TetrisManager *manager);
int evaluate(TetrisManager *manager);
uint16_t getBestPlacing(const TetrisManager *manager);

// =============================================================================
void putDownTetris(TetrisManager *manager) {
	removeTetris(manager);
	for (; manager->y < ROW_END; ++manager->y) {
		if (checkCollision(manager)) {
			break;
		}
	}
	--manager->y;
	insertTetris(manager);
}

// =============================================================================
int calcLanding(TetrisManager *manager) {
	int8_t x, y, k, count, cells;
	int8_t height = 25 - manager->y;
	uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

	if ((tetris & 0xF000) == 0) {
		--height;
	}
	if ((tetris & 0xFF00) == 0) {
		--height;
	}
	if ((tetris & 0xFFF0) == 0) {
		--height;
	}

	cells = 0;
	count = 0;
	k = 0;
	y = manager->y + 3;
	do {
		if (y < ROW_END && manager->pool[y] == 0xFFFFU) {
			for (x = 0; x < 4; ++x) {
				if (((tetris >> (k << 2)) >> x) & 1) {
					++cells;
				}
			}
			++count;

			memmove(manager->pool + 1, manager->pool, sizeof(uint16_t) * y);
		} else {
			--y;
			++k;
		}
	} while (y >= manager->y && k < 4);

	height -= count;

	return count * cells * 34 - 45 * height;
}

// =============================================================================
int calcTrans(const TetrisManager *manager) {
	int8_t x, y;
	int rowTrans = 0, colTrans = 0;
	int filled, test;

	for (y = ROW_BEGIN; y < ROW_END; ++y) {
		filled = 1;
		for (x = COL_BEGIN; x < COL_END; ++x) {
			test = (manager->pool[y] >> x) & 1;
			if (filled != test) {
				++rowTrans;
				filled = test;
			}
		}
		if (filled != 1) {
			++rowTrans;
		}
	}

	for (x = COL_BEGIN; x < COL_END; ++x) {
		filled = 1;
		for (y = ROW_BEGIN; y < ROW_END; ++y) {
			test = (manager->pool[y] >> x) & 1;
			if (filled != test) {
				++colTrans;
				filled = test;
			}
		}
		if (filled != 1) {
			++colTrans;
		}
	}

	return 32 * rowTrans + 93 * colTrans;
}

// =============================================================================
int calcStatus(const TetrisManager *manager) {
	static const int wellDepthTable[29] = {
		0,   1,   3,   6,   10,  15,  21,  28,  36,  45,  55,  66,  78,  91, 105,
		120, 136, 153, 171, 190, 210, 231, 253, 276, 300, 325, 351, 378, 406
	};
	int8_t x, y;
	int holeCount = 0, wellDepthSum, depth;

	for (x = COL_BEGIN; x < COL_END; ++x) {
		for (y = ROW_BEGIN; y < ROW_END; ++y) {
			if ((manager->pool[y] >> x) & 1) {
				break;
			}
		}
		while (y < 26) {
			if (!((manager->pool[y] >> x) & 1)) {
				++holeCount;
			}
			++y;
		}
	}

	wellDepthSum = 0;
	for (x = COL_BEGIN; x < COL_END; ++x) {
		depth = 0;
		for (y = ROW_END - 1; y >= ROW_BEGIN; --y) {
			if (!((manager->pool[y] >> x) & 1)) {
				if (((manager->pool[y - 1] >> x) & 1) &&
						((manager->pool[y + 1] >> x) & 1)) {
					++depth;
				}
			} else {
				wellDepthSum += wellDepthTable[depth];
				depth = 0;
			}
		}
		wellDepthSum += wellDepthTable[depth];
	}

	return 79 * holeCount + 34 * wellDepthSum;
}

// =============================================================================
inline int evaluate(TetrisManager *manager) {
	putDownTetris(manager);

	return calcLanding(manager) - calcTrans(manager) - calcStatus(manager);
}

// =============================================================================
uint16_t getBestPlacing(const TetrisManager *manager) {
	int8_t i, j, count, type, ori, rotateLimit, deltaX;
	static AIPlacing placing[48];
	AIPlacing *best = NULL;
	static TetrisManager backup;

	memset(placing, 0, sizeof(placing));

	type = manager->type[0];
	ori = manager->orientation[0];

	switch (type) {
		case TETRIS_I:
		case TETRIS_Z:
		case TETRIS_S:
			rotateLimit = 2;
			break;
		case TETRIS_T:
		case TETRIS_L:
		case TETRIS_J:
			rotateLimit = 4;
			break;
		case TETRIS_O:
			rotateLimit = 1;
			break;
		default:
			rotateLimit = 0;
			break;
	}

	count = 0;
	for (i = 0; i < rotateLimit; ++i) {
		for (j = 0; j < 13; ++j) {
			memcpy(&backup, manager, sizeof(TetrisManager));
			removeTetris(&backup);
			backup.orientation[0] = (i + ori) & 3;
			backup.x = j;

			if (!checkCollision(&backup)) {
				placing[count].action = i;
				placing[count].action <<= 8;
				placing[count].action |= j;
				placing[count].value = 1000000 + evaluate(&backup);
				deltaX = j - manager->x;
				if (deltaX > 0) {
					placing[count].priority = 100 * deltaX + i;
				} else {
					placing[count].priority = 100 * (-deltaX) + 10 + i;
				}
				++count;
			}
		}
	}

	best = placing;
	for (i = 1; i < count; ++i) {
		if (placing[i].value > best->value) {
			best = placing + i;
		} else if (placing[i].value == best->value) {
			if (placing[i].priority > best->priority) {
				best = placing + i;
			}
		}
	}

	return best->action;
}

// =============================================================================
void autoRun(TetrisManager *manager, TetrisControl *control) {
	uint16_t best;
	int8_t i, rotate, destX, deltaX, attemptMoveDirection;
	bool state; // Whether the last action succeeded
	signed long delayTime = 100;
	if (easterEgg[2]) {
		delayTime = 0;
	}
	control->model = 1;
	control->frameRate = delayTime;
	int ch;

	printTetrisPool(manager, control);

	Sleep(1000);
	while (!control->dead) {
		while (_kbhit()) {
			ch = _getch();
			switch (ch) {
				case 27:
					return;
				case 32:
				case 13:
					control->pause = !control->pause;
					gotoxyWithFullwidth(27, 12);
					SetConsoleTextAttribute(hConsoleOutput, 0x0B);
					if (control->pause) {
						buffer_print("■");
					} else {
						buffer_print("□");
					}
					flushPrint();
					break;
				case 72:
				case 'W':
				case '+':
					delayTime /= 2;
					if (delayTime < 0) {
						delayTime = 0;
					}
					control->frameRate = delayTime;
					break;
				case 80:
				case 'S':
				case '-':
					if (easterEgg[2]) {
						break;
					}
					delayTime = (delayTime + 1) * 2;
					if (delayTime > 300) {
						delayTime = 300;
					}
					control->frameRate = delayTime;
					break;
				default:
					break;
			}
		}

		if (control->pause) {
			continue;
		}

		for (i = 0; i < 3; ++i) {
			moveDownTetris(manager, control);
		}

		best = getBestPlacing(manager);
		rotate = (best >> 8);
		destX = (best & 0x0F);
		deltaX = destX - manager->x;

		attemptMoveDirection = 0;
		for (i = 0; i < rotate; ++i) {
			Sleep(delayTime);
			Sleep(delayTime);
			state = keydownControl(manager, control, 'w');
			if (!state) {
				--i;
				if (attemptMoveDirection == 0) {
					state = keydownControl(manager, control, 'a');
					if (!state) {
						attemptMoveDirection = 1;
					}
				}
				if (attemptMoveDirection == 1) {
					keydownControl(manager, control, 'w');
				}
			}
		}

		Sleep(delayTime);

		if (deltaX > 0) {
			for (i = 0; i < deltaX; ++i) {
				keydownControl(manager, control, 'd');
				Sleep(delayTime);
			}
		} else if (deltaX < 0) {
			for (i = 0; i < -deltaX; ++i) {
				keydownControl(manager, control, 'a');
				Sleep(delayTime);
			}
		}

		keydownControl(manager, control, ' ');
		Sleep(delayTime);
	}
}

signed long benchmarkRun(TetrisManager *manager, TetrisControl *control) {
	uint16_t best;
	int8_t i, rotate, destX, deltaX;
	int ch;
	signed long mark;

	control->model = 1;
	printTetrisPool(manager, control);

	while (!control->dead) {
		mark = calcFPS();
		if (mark < 0) {
			return -mark;
		}
		while (_kbhit()) {
			ch = _getch();
			switch (ch) {
				case 27:
					return mark < 0 ? -mark : mark;
			}
		}

		for (i = 0; i < 3; ++i) {
			moveDownTetris(manager, control);
		}

		best = getBestPlacing(manager);
		rotate = (best >> 8);
		destX = (best & 0x0F);
		deltaX = destX - manager->x;

		for (i = 0; i < rotate; ++i) {
			keydownControl(manager, control, 'w');
		}

		if (deltaX > 0) {
			for (i = 0; i < deltaX; ++i) {
				keydownControl(manager, control, 'd');
			}
		} else if (deltaX < 0) {
			for (i = 0; i < -deltaX; ++i) {
				keydownControl(manager, control, 'a');
			}
		}

		keydownControl(manager, control, ' ');
	}
	return -1;
}

inline double getTime(void) {
	static LARGE_INTEGER freq, counter;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&counter);
	return 1.0e3 * counter.QuadPart / freq.QuadPart;
}

inline signed long calcFPS(void) {
	static double fraps;
	static unsigned long framesElapsed = 0;
	static unsigned long totalFramesElapsed = 0;
	static unsigned long mark,
		   markMax = 0, markMaxLesser = 0;
	static unsigned long markMaxDuration =
		0;

	static double lastRecordedTime = 0.0;
	static double lastDisplayTime = 0.0;

	if (general.resetFpsCounter) {
		general.resetFpsCounter = false;
		framesElapsed = 0;
		totalFramesElapsed = 0;
		markMax = 0;
		markMaxLesser = 0;
		markMaxDuration = 0;
	}

	framesElapsed++;
	totalFramesElapsed++;
	lastRecordedTime = getTime();

	// No action in manual mode
	if (general.model == 0)
		return 0;

	if (lastRecordedTime - (double)fpsRate > lastDisplayTime) {
		fraps = framesElapsed * 1000.0 / (lastRecordedTime - lastDisplayTime);
		mark = (unsigned long)(totalFramesElapsed * 100000.0 /
							   (lastRecordedTime - general.startTime));

		buffer_SetConsoleTextAttribute(hConsoleOutput, 0x0B);
		if (general.benchmark) {
			gotoxyWithFullwidth(32, 20);
			buffer_printf("%6lu%4s", mark, "");
			if (mark > markMax) {
				markMaxDuration = 0;
				markMaxLesser = markMax;
				markMax = mark;
				gotoxyWithFullwidth(27, 21);
				buffer_printf("%6lu%4s", markMaxLesser, "");
			} else if (mark > markMaxLesser) {
				markMaxDuration++;
				markMaxLesser = mark;
				gotoxyWithFullwidth(27, 21);
				buffer_printf("%6lu%4s", markMaxLesser, "");
			} else {
				markMaxDuration++;
				if (markMaxDuration > 20) {
					return -(signed long)markMaxLesser; // End Benchmark mode
				}
			}
		} else if (fraps > 9999.48) {
			SetConsoleTextAttribute(hConsoleOutput, 0x0C);
			gotoxyWithFullwidth(27, 21);
			buffer_print("Maxed Out!");
		} else {
			gotoxyWithFullwidth(27, 21);
			buffer_printf("%6.2lf fps", fraps);
		}
		lastDisplayTime = lastRecordedTime;
		framesElapsed = 0;
	}
	return markMax;
}

inline void pause(void) {
	while (_kbhit())
		_getch();
	while (!_kbhit())
		;
	while (_kbhit())
		_getch();
}

inline void clrscr(void) {
	int i;
	for (i = 0; i < 2000; i++) {
		outputBuffer[outputCursorPosition.Y][outputCursorPosition.X]
		.Char.AsciiChar = '\0';
		outputBuffer[outputCursorPosition.Y][outputCursorPosition.X].Attributes =
			0x0F;
		IncrementOutputCursorPosition();
	}
	flushPrint();
}

inline void flushPrint() {
	SMALL_RECT outputWriteRegion = outputRegion;
	calcFPS();
	WriteConsoleOutput(hConsoleOutput, (const CHAR_INFO *)outputBuffer,
					   outputBufferSize, zeroPosition, &outputWriteRegion);
}
inline void buffer_SetConsoleCursorPosition(HANDLE handle, COORD Pos) {
	outputCursorPosition.X = Pos.X;
	outputCursorPosition.Y = Pos.Y;
}
inline void buffer_SetConsoleTextAttribute(HANDLE handle, WORD Attribute) {
	outputAttribute = Attribute;
}
void buffer_print(LPCSTR String) {
	while (*String != '\0') {
		outputBuffer[outputCursorPosition.Y][outputCursorPosition.X]
		.Char.AsciiChar = *String;
		outputBuffer[outputCursorPosition.Y][outputCursorPosition.X].Attributes =
			outputAttribute;
		++String;
		IncrementOutputCursorPosition();
	}
}
inline void IncrementOutputCursorPosition() {
	outputCursorPosition.X++;
	if (outputCursorPosition.X >= outputBufferSize.X) {
		outputCursorPosition.X = 0;
		outputCursorPosition.Y++;
		if (outputCursorPosition.Y >= outputBufferSize.Y) {
			outputCursorPosition.Y = 0;
		}
	}
}

bool enableDebugPrivilege() {
	HANDLE hToken;
	LUID sedebugnameValue;
	TOKEN_PRIVILEGES tkp;
	if (!OpenProcessToken(GetCurrentProcess(),
						  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		return false;
	}
	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue)) {
		CloseHandle(hToken);
		return false;
	}
	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Luid = sedebugnameValue;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL)) {
		CloseHandle(hToken);
		return false;
	}
	return true;
}

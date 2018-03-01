#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

const char* const INFO_AUTHOR = "GeForce GTX 982 Ti";
const char* const INFO_VERSION = "1.8 c"; 


// 方块形状
enum { TETRIS_I = 0, TETRIS_T, TETRIS_L, TETRIS_J, TETRIS_Z, TETRIS_S, TETRIS_O };

// =============================================================================
// 7种方块的4旋转状态（4位为一行）
static const uint16_t gs_uTetrisTable[7][4] =
{
    { 0x00F0U, 0x2222U, 0x00F0U, 0x2222U },  // I型
    { 0x0072U, 0x0262U, 0x0270U, 0x0232U },  // T型
    { 0x0223U, 0x0074U, 0x0622U, 0x0170U },  // L型
    { 0x0226U, 0x0470U, 0x0322U, 0x0071U },  // J型
    { 0x0063U, 0x0264U, 0x0063U, 0x0264U },  // Z型
    { 0x006CU, 0x0462U, 0x006CU, 0x0462U },  // S型
    { 0x0660U, 0x0660U, 0x0660U, 0x0660U }   // O型
};

// =============================================================================
// 初始状态的游戏池
// 每个元素表示游戏池的一行，下标大的是游戏池底部
// 两端各置2个1，底部2全置为1，便于进行碰撞检测
// 由于左端只有2个1，要保证所有方块的x坐标大于0，否则位操作时会出BUG
// 这样一来游戏池的宽度为12列
// 如果想要传统的10列，只需多填两个1即可（0xE007），当然显示相关部分也要随之改动
// 当某个元素为0xFFFFU时，说明该行已被填满
// 顶部4行用于给方块，不显示出来
// 再除去底部2行，显示出来的游戏池高度为22行
static const uint16_t gs_uInitialTetrisPool[28] =
{
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
typedef struct TetrisManager  // 这个结构体存储游戏相关数据
{
    uint16_t pool[28];  // 游戏池
    int8_t x;  // 当前方块x坐标，此处坐标为方块左上角坐标
    int8_t y;  // 当前方块y坐标
    int8_t type[3];  // 当前、下一个和下下一个方块类型
    int8_t orientation[3];  // 当前、下一个和下下一个方块旋转状态
} TetrisManager;

// =============================================================================
typedef struct TetrisControl  // 这个结构体存储控制相关数据
{
    // 游戏池内每格的颜色
    // 由于此版本是彩色的，仅用游戏池数据无法存储颜色信息
    // 当然，如果只实现单色版的，就没必要用这个数组了
    int8_t color[28][16];
    bool dead;  // 挂
    bool pause;  // 暂停
    bool clockwise;  // 旋转方向：顺时针为true
    int8_t direction;  // 移动方向：0向左移动 1向右移动
    bool model;  // 是否开启AI模式 
    uint16_t frameRate;  // 两帧之间的时间 
    unsigned score;  // 得分
    unsigned erasedCount[4];  // 消行数
    unsigned erasedTotal;  // 消行总数
    unsigned tetrisCount[7];  // 各方块数
    unsigned tetrisTotal;  // 方块总数
} TetrisControl;

HANDLE g_hConsoleOutput;  // 控制台句柄
struct{
	int8_t model;  // 游戏运行模式
	int8_t benchmark;  // 评价跑分模式 
	double startTime;  // 内置计时器 
	bool resetFpsCounter; 
} general; 

unsigned long fpsRate = 800; 
// =============================================================================
// 彩蛋：
// [0] 大师向泽耍淫威
// [1] 谢文皓喜欢叶子
// [2] Turbo Boost
// [3] 调频 
#define easterNum 4
int8_t easterEgg[easterNum] = {0}; 

// =============================================================================
// 函数声明
// 如果使用全局变量方式实现，就没必要传参了
void autoRun(TetrisManager *manager, TetrisControl *control);  // 自动运行
void benchmark(TetrisManager *manager, TetrisControl *control);  // 跑分运行
signed long benchmarkRun(TetrisManager *manager, TetrisControl *control);  // 跑分核心 
signed long calcFPS(void);  // 计算FPS，返回Benchmark模式下最大评分 
bool checkCollision(const TetrisManager *manager);  // 碰撞检测
bool checkErasing(TetrisManager *manager, TetrisControl *control);  // 消行检测
void clrscr(void);  // 清屏 
double getTime(void);  // 高精度计时器 
void dropDownTetris(TetrisManager *manager, TetrisControl *control);  // 方块直接落地
void giveTetris(TetrisManager *manager, TetrisControl *control);  // 给一个方块
void gotoxyInPool(short x, short y); // 定位到游戏池 
void gotoxyWithFullwidth(short x, short y);  // 以全角定位到某点
void horzMoveTetris(TetrisManager *manager, TetrisControl *control);  // 水平移动方块
void initGame(TetrisManager *manager, TetrisControl *control, bool model);  // 初始化游戏
void insertTetris(TetrisManager *manager);  // 插入方块
void keydownControl(TetrisManager *manager, TetrisControl *control, int key);  // 键按下
int mainMenu(void);  // 主菜单
void moveDownTetris(TetrisManager *manager, TetrisControl *control);  // 向下移动方块
void pause(void);  // 暂停 
void printCurrentTetris(const TetrisManager *manager, const TetrisControl *control);  // 显示当前方块
void printNextTetris(const TetrisManager *manager);  // 显示下一个和下下一个方块
void printPoolBorder();  // 显示游戏池边界
void printPrompting(const TetrisControl *control);  // 显示提示信息
void printScore(const TetrisManager *manager, const TetrisControl *control);  // 显示得分信息
void printTetrisPool(const TetrisManager *manager, const TetrisControl *control);  // 显示游戏池
void removeTetris(TetrisManager *manager);  // 移除方块
void rotateTetris(TetrisManager *manager, TetrisControl *control);  // 旋转方块
void runGame(TetrisManager *manager, TetrisControl *control);  // 运行游戏
void setPoolColor(const TetrisManager *manager, TetrisControl *control);  // 设置颜色

// =============================================================================
// 主函数
int main(int argc, char* argv[])
{
	// 如果程序运行时获得了更多参数 
	if (argc > 1)
	{
		char optionEx[64]; 
		for (int i = 1;i < argc;i++)
		{
			if (argv[i][0] == '/' || argv[i][0] == '-')
			{
				argv[i]++;
				// 把参数转换成小写 
				char *a = argv[i];
				while (*a != '\0' && *a != ':')
				{
					if (*a >= 'A' && *a <= 'Z')
					{
						*a += 0x20;
					}
					a++;
				}
				if (*a == ':')
				{
					*a = '\0';
					a++;
					strcpy(optionEx, a);
				}
				
				if (strcmp(argv[i], "c16") == 0)
				{
					easterEgg[1] = 1;
				}
				else if (strcmp(argv[i], "boost") == 0 || strcmp(argv[i], "turbo") == 0)
				{
					easterEgg[2] = 1;
				}
				else if (strcmp(argv[i], "fpsrate") == 0)
				{
					long newfps = atol(optionEx);
					if (newfps > 0){
						fpsRate = newfps;
						if (fpsRate > 5000)
						{
							fpsRate = 5000;
						}
						easterEgg[3] = 1;
					}
				}
				else
				{
					printf("Error: Unknown command line option \"%s\"\n", argv[i]);
					return 1; 
				}
			}
		}
	}
	
	// 初始化控制台窗口 
    TetrisManager tetrisManager;
    TetrisControl tetrisControl;
    g_hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);  // 获取控制台输出句柄
    CONSOLE_CURSOR_INFO cursorInfo = { 1, FALSE };  // 光标信息
    CONSOLE_SCREEN_BUFFER_INFO screenInfo;  // 控制台屏幕信息 
	
	GetConsoleScreenBufferInfo(g_hConsoleOutput, &screenInfo);
	COORD bufferSize;
	bufferSize.X = 1 + screenInfo.srWindow.Right;
	bufferSize.Y = 1 + screenInfo.srWindow.Bottom;
	SetConsoleScreenBufferSize(g_hConsoleOutput, bufferSize); 
	clrscr();
	
    SetConsoleCursorInfo(g_hConsoleOutput, &cursorInfo);  // 设置光标隐藏
    char conTitle[64] = {0};
    snprintf(conTitle, sizeof(conTitle), "高能俄罗斯方块Ver %s",INFO_VERSION);
    SetConsoleTitleA(conTitle);
    
    DeleteMenu(GetSystemMenu(GetConsoleWindow(),0),SC_MAXIMIZE,MF_DISABLED);
	
    do
    {
        general.model = mainMenu();
        if (general.model != 2)
        {
        	general.benchmark = 0;
        }
        else
        {
        	general.benchmark = 1;
        }
        SetConsoleTextAttribute(g_hConsoleOutput, 0x07);
        clrscr(); 
        initGame(&tetrisManager, &tetrisControl, general.model == 0);  // 初始化游戏
        printPrompting(&tetrisControl);  // 显示提示信息
        printPoolBorder();  // 显示游戏池边界
		
		int8_t prevEE2;
        switch (general.model)
        {
        case 0: 
        	tetrisControl.frameRate = 450;
            runGame(&tetrisManager, &tetrisControl);  // 运行游戏
            break;
		case 1: 
            autoRun(&tetrisManager, &tetrisControl);  // 自动运行
            break;
        case 2:
        	prevEE2 = easterEgg[2];
			easterEgg[2] = 1; 
    		
        	SetConsoleTextAttribute(g_hConsoleOutput, 0x07);
        	clrscr(); 
        	initGame(&tetrisManager, &tetrisControl, 0);  // 初始化游戏
        	printPrompting(&tetrisControl);  // 显示提示信息
        	printPoolBorder();  // 显示游戏池边界
			signed long mark = benchmarkRun(&tetrisManager, &tetrisControl);
			
			clrscr();
		    SetConsoleTextAttribute(g_hConsoleOutput, 0x0F);
    		gotoxyWithFullwidth(14, 5);
    		printf("┏━━━━━━━━━━┓");
    		gotoxyWithFullwidth(14, 6);
    		printf("┃%3s%s%3s┃", "", "高能俄罗斯方块", "");
    		gotoxyWithFullwidth(14, 7);
    		printf("┃%6s%s%6s┃", "", "跑分模式", "");
    		gotoxyWithFullwidth(14, 8);
    		printf("┗━━━━━━━━━━┛");
    		gotoxyWithFullwidth(14, 9);
			printf("作者：%s", INFO_AUTHOR); 
    		gotoxyWithFullwidth(15, 11);
			printf("你的电脑评分：%6d", mark);
			
			pause();
			easterEgg[2] = prevEE2;
			continue; 
			break;
		default:
			return -1; 
        }

        SetConsoleTextAttribute(g_hConsoleOutput, 0xF0);
        gotoxyWithFullwidth(12, 9);
		printf("%20s\n","");
        gotoxyWithFullwidth(12, 10);
        printf("%2s%s%2s\n","",easterEgg[1] ? " 谢文皓喜欢叶子 " : "按任意键回主菜单","");
        gotoxyWithFullwidth(12, 11);
		printf("%20s\n","");
        SetConsoleTextAttribute(g_hConsoleOutput, 0x07);
        pause();
        clrscr(); 

    } while (1);

    gotoxyWithFullwidth(0, 0);
    CloseHandle(g_hConsoleOutput);
    return 0;
}

// =============================================================================
// 初始化游戏
void initGame(TetrisManager *manager, TetrisControl *control, bool model)
{
    memset(manager, 0, sizeof(TetrisManager));  // 全部置0

    // 初始化游戏池
    memcpy(manager->pool, gs_uInitialTetrisPool, sizeof(uint16_t [28]));
    if (general.benchmark)
    {
    	srand(2014530982);  // 跑分模式下使用默认随机种子 
    }
    else
    {
    	srand((unsigned)time(NULL));  // 设置随机种子
    }

    manager->type[1] = rand() % 7;  // 下一个
    manager->orientation[1] = rand() & 3;
    manager->type[2] = rand() % 7;  // 下下一个
    manager->orientation[2] = rand() & 3;
    if (general.benchmark)
    {
    	manager->type[1] = 0;
    	manager->type[2] = 1;
    }

    memset(control, 0, sizeof(TetrisControl));  // 全部置0
    control->model = model;

    giveTetris(manager, control);  // 给下一个方块
    setPoolColor(manager, control);  // 设置颜色
    printScore(manager, control);  // 显示得分信息
    printTetrisPool(manager, control);  // 清空Sync方法的备份数据 
    general.startTime = getTime();
    general.resetFpsCounter = true;
    calcFPS();
}

// =============================================================================
// 给一个方块
void giveTetris(TetrisManager *manager, TetrisControl *control)
{
    static uint16_t tetris;
	static uint16_t num;
	
    manager->type[0] = manager->type[1];  // 下一个方块置为当前
    manager->orientation[0] = manager->orientation[1];

    manager->type[1] = manager->type[2];// 下下一个置方块为下一个
    manager->orientation[1] = manager->orientation[2];
	
	// 动点手脚，让I形数量稍微多一些 
	num = rand();
	if (num >= 32200)
	{
		manager->type[2] = 0;// 动些手脚 
	}
	else
	{
    	manager->type[2] = rand() % 7;// 随机生成下下一个方块
    }
    if (general.benchmark)
    {
    	manager->type[2] = (manager->type[1] + 1) % 7;
    }
    manager->orientation[2] = rand() & 3;

    tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];  // 当前方块

    // 设置当前方块y坐标，保证刚给出时只显示方块最下面一行
    // 这种实现使得玩家可以以很快的速度将方块落在不显示出来的顶部4行内
    if (tetris & 0xF000)
    {
        manager->y = 0;
    }
    else
    {
        manager->y = (tetris & 0xFF00) ? 1 : 2;
    }
    manager->x = 6;  // 设置当前方块x坐标

    if (checkCollision(manager))  // 检测到碰撞
    {
        control->dead = true;  // 标记游戏结束
    }
    else  // 未检测到碰撞
    {
        insertTetris(manager);  // 将当前方块加入游戏池
    }

    ++control->tetrisTotal;  // 方块总数
    ++control->tetrisCount[manager->type[0]];  // 相应方块数

    printNextTetris(manager);  // 显示下一个方块
}

// =============================================================================
// 碰撞检测
bool checkCollision(const TetrisManager *manager)
{
    // 当前方块
    uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];
    uint16_t dest = 0U;

    // 获取当前方块在游戏池中的区域：
    // 游戏池坐标x y处小方格信息，按低到高存放在16位无符号数中
    dest |= (((manager->pool[manager->y + 0] >> manager->x) << 0x0) & 0x000F);
    dest |= (((manager->pool[manager->y + 1] >> manager->x) << 0x4) & 0x00F0);
    dest |= (((manager->pool[manager->y + 2] >> manager->x) << 0x8) & 0x0F00);
    dest |= (((manager->pool[manager->y + 3] >> manager->x) << 0xC) & 0xF000);

    // 若当前方块与目标区域存在重叠（碰撞），则位与的结果不为0
    return ((dest & tetris) != 0);
}

// =============================================================================
// 插入方块
void insertTetris(TetrisManager *manager)
{
    // 当前方块
    uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

    // 当前方块每4位取出，位或到游戏池相应位置，即完成插入方块
    manager->pool[manager->y + 0] |= (((tetris >> 0x0) & 0x000F) << manager->x);
    manager->pool[manager->y + 1] |= (((tetris >> 0x4) & 0x000F) << manager->x);
    manager->pool[manager->y + 2] |= (((tetris >> 0x8) & 0x000F) << manager->x);
    manager->pool[manager->y + 3] |= (((tetris >> 0xC) & 0x000F) << manager->x);
}

// =============================================================================
// 移除方块
void removeTetris(TetrisManager *manager)
{
    // 当前方块
    uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

    // 当前方块每4位取出，按位取反后位与到游戏池相应位置，即完成移除方块
    manager->pool[manager->y + 0] &= ~(((tetris >> 0x0) & 0x000F) << manager->x);
    manager->pool[manager->y + 1] &= ~(((tetris >> 0x4) & 0x000F) << manager->x);
    manager->pool[manager->y + 2] &= ~(((tetris >> 0x8) & 0x000F) << manager->x);
    manager->pool[manager->y + 3] &= ~(((tetris >> 0xC) & 0x000F) << manager->x);
}

// =============================================================================
// 设置颜色
void setPoolColor(const TetrisManager *manager, TetrisControl *control)
{
    // 由于显示游戏池时，先要在游戏池里判断某一方格有方块才显示相应方格的颜色
    // 这里只作设置即可，没必要清除
    // 当移动方块或给一个方块时调用

    int8_t i, x, y;

    // 当前方块
    uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

    for (i = 0; i < 16; ++i)
    {
        y = (i >> 2) + manager->y;  // 待设置的列
        if (y > ROW_END)  // 超过底部限制
        {
            break;
        }
        x = (i & 3) + manager->x;  // 待设置的行
        if ((tetris >> i) & 1)  // 检测的到小方格属于当前方块区域
        {
            control->color[y][x] = (manager->type[0] | 8);  // 设置颜色
        }
    }
}

// =============================================================================
// 旋转方块
void rotateTetris(TetrisManager *manager, TetrisControl *control)
{
    int8_t ori = manager->orientation[0];  // 记录原旋转状态

    removeTetris(manager);  // 移走当前方块

    // 顺/逆时针旋转
    manager->orientation[0] = (control->clockwise) ? ((ori + 1) & 3) : ((ori + 3) & 3);

    if (checkCollision(manager))  // 检测到碰撞
    {
        manager->orientation[0] = ori;  // 恢复为原旋转状态
        insertTetris(manager);  // 放入当前方块。由于状态没改变，不需要设置颜色
    }
    else
    {
        insertTetris(manager);  // 放入当前方块
        setPoolColor(manager, control);  // 设置颜色
        if (!easterEgg[2] || control->model == 0)
        {
        	printCurrentTetris(manager, control);  // 显示当前方块
        }
    }
}

// =============================================================================
// 水平移动方块
void horzMoveTetris(TetrisManager *manager, TetrisControl *control)
{
    int x = manager->x;  // 记录原列位置

    removeTetris(manager);  // 移走当前方块
    control->direction == 0 ? (--manager->x) : (++manager->x);  // 左/右移动

    if (checkCollision(manager))  // 检测到碰撞
    {
        manager->x = x;  // 恢复为原列位置
        insertTetris(manager);  // 放入当前方块。由于位置没改变，不需要设置颜色
    }
    else
    {
        insertTetris(manager);  // 放入当前方块
        setPoolColor(manager, control);  // 设置颜色
        if (!easterEgg[2] || !control->model)
        {
        	printCurrentTetris(manager, control);  // 显示当前方块
        }
    }
}

// =============================================================================
// 向下移动方块
void moveDownTetris(TetrisManager *manager, TetrisControl *control)
{
    int8_t y = manager->y;  // 记录原行位置

    removeTetris(manager);  // 移走当前方块
    ++manager->y;  // 向下移动

    if (checkCollision(manager))  // 检测到碰撞
    {
        manager->y = y;  // 恢复为原行位置
        insertTetris(manager);  // 放入当前方块。由于位置没改变，不需要设置颜色
        if (checkErasing(manager, control))  // 检测到消行
        {
			if (control->frameRate > 0)
			{
				Sleep(2 * control->frameRate);
    			calcFPS();  // 为了保证FPS的准确率 
			}
            printTetrisPool(manager, control);  // 显示游戏池
        }
    }
    else
    {
        insertTetris(manager);  // 放入当前方块
        setPoolColor(manager, control);  // 设置颜色
        printCurrentTetris(manager, control);  // 显示当前方块
    }
}

// =============================================================================
// 方块直接落地
void dropDownTetris(TetrisManager *manager, TetrisControl *control)
{
    removeTetris(manager);  // 移走当前方块

    // 从上往下检测
    // 注意这里不能从下往上，否则会出现方块穿过盖埋入空洞的BUG
    for (; manager->y < ROW_END; ++manager->y)
    {
        if (checkCollision(manager))  // 检测到碰撞
        {
            break;
        }
    }
    --manager->y;  // 上移一格当然没有碰撞

    insertTetris(manager);  // 放入当前方块
    setPoolColor(manager, control);  // 设置颜色

    printTetrisPool(manager, control);  // 先显示游戏池
    if (checkErasing(manager, control) && control->frameRate > 0)  // 检测消行并缓冲 
	{
		Sleep(2 * control->frameRate);
    	printTetrisPool(manager, control);  // 再显示一遍游戏池
    	calcFPS();  // 为了保证FPS的准确率 
	}
}

// =============================================================================
// 消行检测
bool checkErasing(TetrisManager *manager, TetrisControl *control)
{
    static const unsigned scores[5] = { 0, 10, 30, 90, 150};  // 消行得分
    static const unsigned scoreRatios[5] = { 0, 1, 5, 15, 25};  // 消行高度奖励分
    int8_t lowest = 0;
    int8_t count = 0;
    int8_t k = 0, y = manager->y + 3;

    do  // 从下往上检测
    {
        if (y < ROW_END && manager->pool[y] == 0xFFFFU)  // 有效区域内且一行已填满
        {
            count++;
            // 记录消掉的行
			lowest = ROW_END - y; 
            // 消除一行方块
            memmove(manager->pool + 1, manager->pool, sizeof(uint16_t) * y);
            // 颜色数组的元素随之移动
            memmove(control->color[1], control->color[0], sizeof(int8_t [16]) * y);
        }
        else
        {
            --y;
            ++k;
        }
    } while (y >= manager->y && k < 4);
	
	lowest--;
    control->erasedTotal += count;  // 消行总数
    control->score += scores[count] + lowest * scoreRatios[count] + 1;  // 得分

    if (count > 0)
    {
        ++control->erasedCount[count - 1];  // 消行
    }

    giveTetris(manager, control);  // 给下一个方块
    setPoolColor(manager, control);  // 设置颜色
    printScore(manager, control);  // 显示得分信息

    return (count > 0);
}

// =============================================================================
// 键按下
void keydownControl(TetrisManager *manager, TetrisControl *control, int key)
{
	static signed char lastAction;
	// if (general.model == 1)
	// {
		calcFPS();
	// }
	
	if (general.model == 0 && lastAction >= 0)
	{
		gotoxyWithFullwidth(27, 10 + lastAction);
		SetConsoleTextAttribute(g_hConsoleOutput, 0x0B);
		printf("□");
	}
	
    if (key == 13)  // 暂停/解除暂停
    {
    	lastAction = 6;
    	
        control->pause = !control->pause;
        
        gotoxyWithFullwidth(27, 10 + lastAction);
		SetConsoleTextAttribute(g_hConsoleOutput, 0x0B);
		if(control->pause)
		{
			printf("■");
		}
		else
		{
			printf("□");
		}
    }

    if (control->pause)  // 暂停状态，不作处理
    {
        return;
    }

    switch (key)
    {
    case 'w': case 'W': case 72:  // 上
    	lastAction = 3;
        control->clockwise = true;  // 顺时针旋转
        rotateTetris(manager, control);  // 旋转方块
        break;
    case 'a': case 'A': case 75:  // 左
    	lastAction = 0;
        control->direction = 0;  // 向左移动
        horzMoveTetris(manager, control);  // 水平移动方块
        break;
    case 'd': case 'D': case 77:  // 右
    	lastAction = 1;
        control->direction = 1;  // 向右移动
        horzMoveTetris(manager, control);  // 水平移动方块
        break;
    case 's': case 'S': case 80:  // 下
    	lastAction = 2;
        moveDownTetris(manager, control);  // 向下移动方块
        break;
    case ' ':  // 直接落地
    	lastAction = 5;
        dropDownTetris(manager, control);
        break;
    case 'x': case 'X': case '0':  // 反转
    	lastAction = 4;
        control->clockwise = false;  // 逆时针旋转
        rotateTetris(manager, control);  // 旋转方块
        break;
    default:
    	lastAction = -1;
        break;
    }
    
    if (general.model == 0 && lastAction >= 0)
	{
    	gotoxyWithFullwidth(27, 10 + lastAction);
		SetConsoleTextAttribute(g_hConsoleOutput, 0x0B);
		printf("■");
	}
}

// =============================================================================
// 以全角定位到某点
inline void gotoxyWithFullwidth(short x, short y)
{
    static COORD cd;

    cd.X = (short)(x << 1);
    cd.Y = y;
    SetConsoleCursorPosition(g_hConsoleOutput, cd);
}

// =============================================================================
// 主菜单
int mainMenu()
{
	static int indexTotal = 3; 
	if (easterEgg[3])
	{
		indexTotal = 2; // 如果启用了调频就屏蔽跑分模式 
	}
    static const char *const modelItem[] = { "1.自己动手的垃圾尝试", "2.显卡先生的神迹再现", "3.无敌至尊的跑分模式" };
    #define secretNum 3
    static const char *const secretCode[secretNum] = {"xzsyw", "xwhxhyz", "boost"};
    static char secretKey[1024];
    signed long int index = 0, secretIndex = 0, ch;
    
    SetConsoleTextAttribute(g_hConsoleOutput, 0x0F);
    gotoxyWithFullwidth(14, 5);
    printf("┏━━━━━━━━━━┓");
    gotoxyWithFullwidth(14, 6);
    printf("┃%3s%s%3s┃", "", "高能俄罗斯方块", "");
    gotoxyWithFullwidth(14, 7);
    printf("┃%3sVersion: %s", "", INFO_VERSION);
    gotoxyWithFullwidth(25, 7);
    printf("┃");
    gotoxyWithFullwidth(14, 8);
    printf("┗━━━━━━━━━━┛");
	gotoxyWithFullwidth(14, 9);
	printf("作者：%s", INFO_AUTHOR); 
	
    SetConsoleTextAttribute(g_hConsoleOutput, 0xF0);
	for (int i = 0;i < indexTotal;i++)
	{
    	gotoxyWithFullwidth(14, 14 + 2 * i);
    	printf("%2s%s%2s", "", modelItem[i], "");
    	SetConsoleTextAttribute(g_hConsoleOutput, 0x0F);
    }
    
    do
    {
        ch = _getch();
        switch (ch)
        {
        /* 
        // 为了SecretCode只能先这样 
        case 'w': case 'W': case '8': case 72:  // 上
        case 'a': case 'A': case '4': case 75:  // 左
        case 'd': case 'D': case '6': case 77:  // 右
        case 's': case 'S': case '2': case 80:  // 下
        */
        
        //===========================================
        case 72: case 75:  // 上左 
		case 77: case 80:  // 右下 
            SetConsoleTextAttribute(g_hConsoleOutput, 0x0F);
            gotoxyWithFullwidth(14, 14 + 2 * index);
            printf("%2s%s%2s", "", modelItem[index], "");
			if (ch == 72 || ch == 75)
			{
                index--;
                if (index < 0)
				{
					index += indexTotal;
				}
            }
			if (ch == 77 || ch == 80)
			{
                index++;
                if (index >= indexTotal)
				{
					index -= indexTotal;
				}
            }
            SetConsoleTextAttribute(g_hConsoleOutput, 0xF0);
            gotoxyWithFullwidth(14, 14 + 2 * index);
            printf("%2s%s%2s", "", modelItem[index], "");
            break;
        case ' ': case 13:  // 空格和Enter 
            return index;
        case 27:  // Esc键直接退出 
        	exit(0);
        	return 0;
        case 8:  // Backspace键清除已输入的秘密代码 
        	memset(secretKey, 0, sizeof(secretKey));
        	secretIndex = 0;
        	break;
        default:
        	if (ch >= 'A' && ch <= 'Z')
        	{
        		ch += 0x20;
        	}
        	if (ch < 'a' || ch > 'z')
        	{
        		break;
        	}
        	secretKey[secretIndex] = ch;
        	secretIndex++;
        	for (int i = 0;i < secretNum;i++)
			{
				if (strcmp(secretKey, secretCode[i]) == 0)
				{
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
// 显示游戏池边界
void printPoolBorder()
{
    int8_t y;

    SetConsoleTextAttribute(g_hConsoleOutput, 0xF0);
    for (y = ROW_BEGIN; y < ROW_END; ++y)  // 不显示顶部4行和底部2行
    {
        gotoxyWithFullwidth(10, y - 3);
        printf("%2s", "");
        gotoxyWithFullwidth(23, y - 3);
        printf("%2s", "");
    }

    gotoxyWithFullwidth(10, y - 3);  // 底部边界
    printf("%28s", "");
}

// 定位到游戏池中的方格
inline void gotoxyInPool(short x, short y)
{
	gotoxyWithFullwidth(x + 9, y - 3);
}

// =============================================================================
// 显示游戏池
// 显卡先生的神迹：Sync显示方法回归！
static uint16_t ppool[16][28] = {0}; 

void printTetrisPool(const TetrisManager *manager, const TetrisControl *control)
{
    int8_t x, y;
    printNextTetris(manager); 

    for (y = ROW_BEGIN; y < ROW_END; ++y)  // 不显示顶部4行和底部2行
    {
        for (x = COL_BEGIN; x < COL_END; ++x)  // 不显示左右边界
        {
            if ((manager->pool[y] >> x) & 1)  // 游戏池该方格有方块
            {
        		if (control->color[y][x] == ppool[x][y])continue; // 方块未变化则跳过 
                // 用相应颜色，显示一个实心方块
				gotoxyInPool(x, y);
                SetConsoleTextAttribute(g_hConsoleOutput, control->color[y][x]);
                printf("■");
                ppool[x][y] = control->color[y][x];
            }
            else  // 没有方块，显示空白
            {
				if (ppool[x][y] == 0)continue; 
                // 多余SetConsoleTextAttribute(g_hConsoleOutput, 0);
				gotoxyInPool(x, y);
                printf("%2s", "");
                ppool[x][y] = 0;
            }
        }
    }
}

// =============================================================================
// 显示当前方块
void printCurrentTetris(const TetrisManager *manager, const TetrisControl *control)
{
    int8_t x, y;

    // 显示当前方块是在移动后调用的，为擦去移动前的方块，需要扩展显示区域
    // 由于不可能向上移动，故不需要向下扩展
    y = (manager->y > ROW_BEGIN) ? (manager->y - 1) : ROW_BEGIN;  // 向上扩展一格
    for (; y < ROW_END && y < manager->y + 4; ++y)
    {
        x = (manager->x > COL_BEGIN) ? (manager->x - 1) : COL_BEGIN;  // 向左扩展一格
        for (; x < COL_END && x < manager->x + 5; ++x)  // 向右扩展一格
        {
            gotoxyInPool(x, y);  // 定点到游戏池中的方格
            if ((manager->pool[y] >> x) & 1)  // 游戏池该方格有方块
            {
                // 用相应颜色，显示一个实心方块
                SetConsoleTextAttribute(g_hConsoleOutput, control->color[y][x]);
                printf("■");
                ppool[x][y] = control->color[y][x];
                
            }
            else  // 没有方块，显示空白
            {
            	if (ppool[x][y] = 0)continue;
                // 多余SetConsoleTextAttribute(g_hConsoleOutput, 0);
                printf("%2s", "");
                ppool[x][y] = 0;
            }
        }
    }
}

// =============================================================================
// 显示下一个和下下一个方块
void printNextTetris(const TetrisManager *manager)
{
    int8_t i;
    uint16_t tetris;


    // 下一个，用相应颜色显示
    tetris = gs_uTetrisTable[manager->type[1]][manager->orientation[1]];
    SetConsoleTextAttribute(g_hConsoleOutput, manager->type[1] | 8);
    for (i = 0; i < 16; ++i)
    {
        gotoxyWithFullwidth((i & 3) + 27, (i >> 2) + 2);
        ((tetris >> i) & 1) ? printf("■") : printf("%2s", "");
    }
	
	if (general.benchmark)
	{
		// 如果是Benchmark跑分模式就不显示下下一个直接跳过 
		return;
	}
	else
	{
    // 下下一个，不显示彩色
    	tetris = gs_uTetrisTable[manager->type[2]][manager->orientation[2]];
    	SetConsoleTextAttribute(g_hConsoleOutput, 8);
    	for (i = 0; i < 16; ++i)
    	{
    	    gotoxyWithFullwidth((i & 3) + 32, (i >> 2) + 2);
    	    ((tetris >> i) & 1) ? printf("■") : printf("%2s", "");
    	}
    }
}

// =============================================================================
// 显示得分信息
void printScore(const TetrisManager *manager, const TetrisControl *control)
{
	static TetrisControl sync; // 高效率Sync实现方法回来了！
	static bool flag; 
    int8_t i;

    SetConsoleTextAttribute(g_hConsoleOutput, 0x0E);
	
	if (control->score != sync.score)
	{ 
    	gotoxyWithFullwidth(5, 6);
    	printf("%u", control->score);
    }
	
	flag = false;
    for (i = 0; i < 4; ++i)
    {
    	if (control->erasedCount[i] != sync.erasedCount[i])
    	{
        	gotoxyWithFullwidth(6, 10 + i);
        	printf("%u", control->erasedCount[i]);
        	flag = true;
        }
    }
    if (flag)
    {
    	gotoxyWithFullwidth(7, 8);
    	printf("%u", control->erasedTotal);
    }
	
	flag = false;
    for (i = 0; i < 7; ++i)
    {
    	if (control->tetrisCount[i] != sync.tetrisCount[i])
    	{
        	gotoxyWithFullwidth(6, 17 + i);
        	printf("%u", control->tetrisCount[i]);
        	flag = true;
        }
    }
    if (flag)
    {
    	gotoxyWithFullwidth(7, 15);
		printf("%u", control->tetrisTotal);
	}
    
    
	sync = *control;
}

// =============================================================================
// 显示提示信息
void printPrompting(const TetrisControl *control)
{
	// 左侧的提示信息 
	static const char* const modelName[] = {"自己动手垃圾尝试", "显卡先生神迹再现"};
	static const char* const easterEggName = "大师向泽来耍淫威";
    static const char* const tetrisName = "ITLJZSO";
    SetConsoleTextAttribute(g_hConsoleOutput, 0x0E);
    
    gotoxyWithFullwidth(1, 1);
    if (general.benchmark)
    {
    	printf("■Benchmark跑分");
    }
    else
    {
    	if (easterEgg[0])
		{
    		SetConsoleTextAttribute(g_hConsoleOutput, 0x0C);
			printf("■%s", easterEggName);
    		SetConsoleTextAttribute(g_hConsoleOutput, 0x0E);
		}
    	printf("■%s", control->model ? modelName[0] : modelName[1]);
    }
    gotoxyWithFullwidth(1, 3);
    printf("□按Esc回主菜单");
    
    gotoxyWithFullwidth(1, 6);
    printf("■得分：%u", control->score);
    gotoxyWithFullwidth(1, 8);
    printf("■消行总数：%u", control->erasedTotal);
    
    int8_t i;
    for (i = 0; i < 4; ++i)
    {
        gotoxyWithFullwidth(2, 10 + i);
        printf("□消%1s%d：%u", "", i + 1, control->erasedCount[i]);
    }
    gotoxyWithFullwidth(1, 15);
    printf("■方块总数：%u", control->tetrisTotal);
    
    for (i = 0; i < 7; ++i)
    {
        gotoxyWithFullwidth(2, 17 + i);
        printf("□%1s%c形：%u", "", tetrisName[i], control->tetrisCount[i]);
    }
    
    // 下一个方块的边框
    SetConsoleTextAttribute(g_hConsoleOutput, 0xF);
    if (general.benchmark)
	{
		// Benchmark跑分模式不显示下面两个 
    	gotoxyWithFullwidth(26, 1);
    	printf("┏━━━━┓");
    	gotoxyWithFullwidth(26, 2);
    	printf("┃%8s┃", "");
    	gotoxyWithFullwidth(26, 3);
    	printf("┃%8s┃", "");
    	gotoxyWithFullwidth(26, 4);
    	printf("┃%8s┃", "");
    	gotoxyWithFullwidth(26, 5);
    	printf("┃%8s┃", "");
    	gotoxyWithFullwidth(26, 6);
    	printf("┗━━━━┛");	
	}
	else
	{
    	gotoxyWithFullwidth(26, 1);
    	printf("┏━━━━┳━━━━┓");
    	gotoxyWithFullwidth(26, 2);
    	printf("┃%8s┃%8s┃", "", "");
    	gotoxyWithFullwidth(26, 3);
    	printf("┃%8s┃%8s┃", "", "");
    	gotoxyWithFullwidth(26, 4);
    	printf("┃%8s┃%8s┃", "", "");
    	gotoxyWithFullwidth(26, 5);
    	printf("┃%8s┃%8s┃", "", "");
    	gotoxyWithFullwidth(26, 6);
    	printf("┗━━━━┻━━━━┛");
    }
    
    SetConsoleTextAttribute(g_hConsoleOutput, 0xB);
    if (!control->model) // AI运行模式 
    {
    
    	if (general.benchmark)
		{ 
    		gotoxyWithFullwidth(26, 8);
    		printf("■跑分模式：");
    		gotoxyWithFullwidth(27, 10);
    		printf("□退出：Esc");
    		
    		SetConsoleTextAttribute(g_hConsoleOutput, 0x0B);
    		gotoxyWithFullwidth(26, 20);
    		printf("■当前评分：%6u", 100 * (unsigned)0.0);
    		gotoxyWithFullwidth(26, 21);
    		printf("■电脑评分：%6u", 100 * (unsigned)0.0);
    	}
    	else
    	{
    		gotoxyWithFullwidth(26, 8);
    		printf("■观看模式：");
    		gotoxyWithFullwidth(27, 10);
    		printf("□加速：↑ W +");
    		gotoxyWithFullwidth(27, 11);
    		printf("□减速：↓ S -");
    		gotoxyWithFullwidth(27, 12);
    		printf("□暂停：Space Enter");
    		gotoxyWithFullwidth(27, 13);
    		printf("□退出：Esc");
    		
    		gotoxyWithFullwidth(26, 21);
    		SetConsoleTextAttribute(g_hConsoleOutput, 0x0B);
    		printf("■实时帧频：%6.2lf fps", 0.0);
    	}
    }
    else
    {
    	gotoxyWithFullwidth(26, 8);
    	printf("■游戏模式：");
    	gotoxyWithFullwidth(27, 10);
    	printf("□向左移动：← A 4");
    	gotoxyWithFullwidth(27, 11);
    	printf("□向右移动：→ D 6");
    	gotoxyWithFullwidth(27, 12);
    	printf("□向下移动：↓ S 2");
    	gotoxyWithFullwidth(27, 13);
    	printf("□顺时针转：↑ W 8");
    	gotoxyWithFullwidth(27, 14);
    	printf("□逆时针转：   X 0");
    	gotoxyWithFullwidth(27, 15);
    	printf("□直接落地：Space");
    	gotoxyWithFullwidth(27, 16);
    	printf("□暂停游戏：Enter");
    	gotoxyWithFullwidth(27, 17);
    	printf("□结束游戏：Esc");
    }
    
    gotoxyWithFullwidth(25, 23);
    printf("■By: %s", INFO_AUTHOR);
}

// =============================================================================
// 运行游戏
void runGame(TetrisManager *manager, TetrisControl *control)
{
    int ch;
    static double clockLast, clockNow;
    control->model = 0; 

    clockLast = getTime();  // 计时
    printTetrisPool(manager, control);  // 显示游戏池

    while (!control->dead)  // 没挂
    {
        while (_kbhit())  // 有键按下
        {
            ch = _getch();
            if (ch == 27)  // Esc键
            {
                return;
            }
            keydownControl(manager, control, ch);  // 处理按键
        }

        if (!control->pause)  // 未暂停
        {
            clockNow = getTime();  // 计时
            // 两次记时的间隔超过0.45秒
            if (clockNow - clockLast > control->frameRate)
            {
                clockLast = clockNow;
                moveDownTetris(manager, control);  // 方块往下移
            }
        }
    }
}

/*******************************************************************************
以下内容中思想来自网络，算法及代码为GeForce GTX 982Ti先生原创 

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

typedef struct _AIPlacing  // 摆法
{
    uint16_t action;  // 操作：高8位是旋转次数，低8位是目标列
    uint16_t priority;  // 优先度
    double value;  // 估值
} AIPlacing;

void putDownTetris(TetrisManager *manager);  // 将方块落到底
int calcLanding(TetrisManager *manager);  // 下落高度和消行数
int calcTrans(const TetrisManager *manager);  // 行列变换
int calcStatus(const TetrisManager *manager);  // 空洞和井
int evaluate(TetrisManager *manager);  // 估值
uint16_t getBestPlacing(const TetrisManager *manager);  // 获取最好摆法

// =============================================================================
// 将方块落到底
void putDownTetris(TetrisManager *manager)
{
    removeTetris(manager);  // 移走当前方块
    for (; manager->y < ROW_END; ++manager->y)  // 从上往下
    {
        if (checkCollision(manager))  // 检测到碰撞
        {
            break;
        }
    }
    --manager->y;  // 上移一格当然没有碰撞
    insertTetris(manager);  // 插入当前方块
}

// =============================================================================
// 下落高度和消行数
int calcLanding(TetrisManager *manager)
{
    int8_t x, y, k, count, cells;
    int8_t height = 25 - manager->y;  // 下落高度
    uint16_t tetris = gs_uTetrisTable[manager->type[0]][manager->orientation[0]];

    if ((tetris & 0xF000) == 0)  // 当前方块最上一行没有方格
    {
        --height;
    }
    if ((tetris & 0xFF00) == 0)  // 当前方块第二行没有方格
    {
        --height;
    }
    if ((tetris & 0xFFF0) == 0)  // 当前方块第三行没有方格
    {
        --height;
    }

    cells = 0;
    count = 0;
    k = 0;
    y = manager->y + 3;  // 从下往上检测
    do
    {
        if (y < ROW_END && manager->pool[y] == 0xFFFFU)  // 有效区域内且一行已填满
        {
            for (x = 0; x < 4; ++x)  // 检测当前方块的对应行
            {
                if (((tetris >> (k << 2)) >> x) & 1)  // 这一行有方块
                {
                    ++cells;  // 消行贡献的方格数
                }
            }
            ++count;
            // 消除一行方块
            memmove(manager->pool + 1, manager->pool, sizeof(uint16_t) * y);
        }
        else
        {
            --y;
            ++k;
        }
    } while (y >= manager->y && k < 4);

    height -= count;  // 再降低下落高度

    return count * cells * 34 - 45 * height;
}

// =============================================================================
// 行列变换
int calcTrans(const TetrisManager *manager)
{
    int8_t x, y;
    int rowTrans = 0, colTrans = 0;
    int filled, test;

    // 行变换
    for (y = ROW_BEGIN; y < ROW_END; ++y)  // 依次检测各行
    {
        filled = 1;  // 游戏池边界算作已填充状态
        for (x = COL_BEGIN; x < COL_END; ++x)
        {
            test = (manager->pool[y] >> x) & 1;  // 检测格的填充状态
            if (filled != test)  // 变换
            {
                ++rowTrans;
                filled = test;
            }
        }
        if (filled != 1)  // 游戏池边界
        {
            ++rowTrans;
        }
    }

    // 列变换
    for (x = COL_BEGIN ; x < COL_END; ++x)  // 依次检测各列
    {
        filled = 1;  // 游戏池边界算作已填充状态
        for (y = ROW_BEGIN; y < ROW_END; ++y)
        {
            test = (manager->pool[y] >> x) & 1;  // 检测格的填充状态
            if (filled != test)  // 变换
            {
                ++colTrans;
                filled = test;
            }
        }
        if (filled != 1)  // 游戏池边界
        {
            ++colTrans;
        }
    }

    return 32 * rowTrans + 93 * colTrans;
}

// =============================================================================
// 空洞和井
int calcStatus(const TetrisManager *manager)
{
    static const int wellDepthTable[29] =
    {
        0, 1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 66, 78,
        91, 105, 120, 136, 153, 171, 190, 210, 231, 253,
        276, 300, 325, 351, 378, 406
    };
    int8_t x, y;
    int holeCount = 0, wellDepthSum, depth;

    for (x = COL_BEGIN; x < COL_END; ++x)  // 行
    {
        for (y = ROW_BEGIN; y < ROW_END; ++y)  // 列
        {
            if ((manager->pool[y] >> x) & 1)  // 有方块
            {
                break;
            }
        }
        while (y < 26)
        {
            if (!((manager->pool[y] >> x) & 1))  // 无方块
            {
                ++holeCount;  // 洞的个数
            }
            ++y;
        }
    }

    wellDepthSum = 0;
    for (x = COL_BEGIN; x < COL_END; ++x)  // 列
    {
        depth = 0;
        for (y = ROW_END - 1; y >= ROW_BEGIN; --y)  // 行，从上往下
        {
            if (!((manager->pool[y] >> x) & 1))  // 无方块
            {
                // 左右两边都有方块
                if (((manager->pool[y - 1] >> x) & 1) && ((manager->pool[y + 1] >> x) & 1))
                {
                    ++depth;
                }
            }
            else
            {
                wellDepthSum += wellDepthTable[depth];
                depth = 0;
            }
        }
        wellDepthSum += wellDepthTable[depth];
    }

    return 79 * holeCount + 34 * wellDepthSum;
}

// =============================================================================
// 估值
inline int evaluate(TetrisManager *manager)
{
    putDownTetris(manager);  // 将方块落到底

    return calcLanding(manager) - calcTrans(manager) - calcStatus(manager);
}

// =============================================================================
// 获取最好摆法
uint16_t getBestPlacing(const TetrisManager *manager)
{
    int8_t i, j, count, type, ori, rotateLimit, deltaX;
    static AIPlacing placing[48];
    AIPlacing *best = NULL;
    static TetrisManager backup;

    memset(placing, 0, sizeof(placing));  // 清0所有摆法

    type = manager->type[0];
    ori = manager->orientation[0];

    switch (type)  // 当前方块类型
    {
    case TETRIS_I: case TETRIS_Z: case TETRIS_S:  // I形、Z形、S形，两种旋转状态
        rotateLimit = 2;
        break;
    case TETRIS_T: case TETRIS_L: case TETRIS_J:  // T形、L形、J形，4种旋转状态
        rotateLimit = 4;
        break;
    case TETRIS_O:  // O形，1种旋转状态
        rotateLimit = 1;
        break;
    default:
        rotateLimit = 0;
        break;
    }

    // 实现未考虑下落一些格后平移来填补空洞，只计算从顶部直接落下的所有摆法
    count = 0;
    for (i = 0; i < rotateLimit; ++i)  // 尝试各种旋转状态
    {
        for (j = 0; j < 13; ++j)  // 尝试每一列
        {
            memcpy(&backup, manager, sizeof(TetrisManager));  // 游戏数据备份
            removeTetris(&backup);  // 移除当前方块
            backup.orientation[0] = (i + ori) & 3;  // 设置旋转状态
            backup.x = j;  // 设置要到达的列

            // 如果检测到碰撞，说明方块根本移不到那一列去
            if (!checkCollision(&backup))  // 未检测到碰撞，得到一种摆法
            {
                placing[count].action = i;
                placing[count].action <<= 8;
                placing[count].action |= j;  // 高8位为旋转状态，低8位为要放置的列
                placing[count].value = 1000000 + evaluate(&backup);  // 估值
                deltaX = j - manager->x;  // 平移的格子数 正为右移，负为左移
                if (deltaX > 0)  // 落于右侧的摆法
                {
                    placing[count].priority = 100 * deltaX + i;  // 优先度
                }
                else  // 落于左侧的摆法
                {
                    placing[count].priority = 100 * (-deltaX) + 10 + i;  // 优先度
                }
                ++count;
            }
        }
    }

    // 算法只考虑当前方块，不递归
    best = placing;
    for (i = 1; i < count; ++i)
    {
        if (placing[i].value > best->value)  // 取估值最高者
        {
            best = placing + i;
        }
        else if (placing[i].value == best->value)  // 估值相同
        {
            if (placing[i].priority > best->priority)  // 取优先度高者
            {
                best = placing + i;
            }
        }
    }

    return best->action;  // 返回摆法
}

// =============================================================================
// 自动运行
void autoRun(TetrisManager *manager, TetrisControl *control)
{
    uint16_t best;
    int8_t i, rotate, destX, deltaX;
    signed long delayTime = 100;
    if (easterEgg[2])
	{
		delayTime = 0;
	}
	control->model = 1; 
    control->frameRate = delayTime; 
    int ch;

    printTetrisPool(manager, control);  // 显示游戏池
	
	// 开始之前稍等一会可以看得更清楚 
	Sleep(1000);
    while (!control->dead)  // 没挂
    {
        while (_kbhit())  // 处理按键
        {
            ch = _getch();
            switch (ch)
            {
            case 27:  // Esc键
                return;
            case 32: case 13:  // 空格和回车键
                control->pause = !control->pause;  // 切换暂停状态
                gotoxyWithFullwidth(27, 12);
                SetConsoleTextAttribute(g_hConsoleOutput, 0x0B);
                if (control->pause)
                {
                	printf("■");
				}
				else
				{
					printf("□");
				}
                break;
            case 72: case 'W': case '+': // 上
                delayTime /= 2;
                if (delayTime < 0)
                {
                    delayTime = 0;
                }
                control->frameRate = delayTime;
                break;
            case 80: case 'S': case '-':  // 下
            	if (easterEgg[2])
            	{
            		break;
            	}
                delayTime = (delayTime + 1) * 2;
                if (delayTime > 300)
                {
                    delayTime = 300;
                }
                control->frameRate = delayTime;
                break;
            default:
				break; 
            }
        }

        if (control->pause)  // 暂停状态
        {
            continue;
        }

        for (i = 0; i < 3; ++i)  // 将给的方块下移，方便观察
        {
            moveDownTetris(manager, control);
        }

        best = getBestPlacing(manager);  // 获取最好摆法
        rotate = (best >> 8);  // 高8位是旋转次数
        destX = (best & 0x0F);  // 低8位是目标列
        deltaX = destX - manager->x;  // 移动的格数，结果为正向右移，负向左移
		
		// 准备动画 
        for (i = 0; i < rotate; ++i)  // 旋转
        {
			Sleep(delayTime);
			calcFPS();  // 为了保证FPS准确性不解释 
			Sleep(delayTime);
            keydownControl(manager, control, 'w');
        }

		Sleep(delayTime);
		
        if (deltaX > 0)
        {
            for (i = 0; i < deltaX; ++i)  // 向右移
            {
                keydownControl(manager, control, 'd');
                Sleep(delayTime);
            }
        }
        else if (deltaX < 0)
        {
            for (i = 0; i < -deltaX; ++i)  // 向左移
            {
                keydownControl(manager, control, 'a');
                Sleep(delayTime);
            }
        }

        keydownControl(manager, control, ' ');  // 移动好后，直接落地
        Sleep(delayTime);
    }
}

signed long benchmarkRun(TetrisManager *manager, TetrisControl *control)
{
    uint16_t best;
    int8_t i, rotate, destX, deltaX;
    int ch;
    signed long mark; 

	control->model = 1; 
    printTetrisPool(manager, control);  // 显示游戏池
	
    while (!control->dead)  // 没挂
    {
        mark = calcFPS();
        if (mark < 0)
        {
            return -mark;
        }
        while (_kbhit())  // 处理按键
        {
            ch = _getch();
            switch (ch)
            {
            case 27: // Esc键
                return mark < 0 ? -mark : mark;
            }
        }

        for (i = 0; i < 3; ++i)  // 将给的方块下移，方便观察
        {
            moveDownTetris(manager, control);
        }

        best = getBestPlacing(manager);  // 获取最好摆法
        rotate = (best >> 8);  // 高8位是旋转次数
        destX = (best & 0x0F);  // 低8位是目标列
        deltaX = destX - manager->x;  // 移动的格数，结果为正向右移，负向左移
		
        for (i = 0; i < rotate; ++i)  // 旋转
        {
            keydownControl(manager, control, 'w');
        }

        if (deltaX > 0)
        {
            for (i = 0; i < deltaX; ++i)  // 向右移
            {
                keydownControl(manager, control, 'd');
            }
        }
        else if (deltaX < 0)
        {
            for (i = 0; i < -deltaX; ++i)  // 向左移
            {
                keydownControl(manager, control, 'a');
            }
        }

        keydownControl(manager, control, ' ');  // 移动好后，直接落地
    }
}

inline double getTime(void)
{
	static LARGE_INTEGER freq, counter; 
    QueryPerformanceFrequency(&freq);   
    QueryPerformanceCounter(&counter);  
    return 1.0e3 * counter.QuadPart / freq.QuadPart;
}

inline signed long calcFPS(void)
{
	static double fraps; // 计算用变量
	static unsigned long framesElapsed = 0; // 新算法使用变量 
	static unsigned long totalFramesElapsed = 0; // Benchmark模式使用变量 
	static unsigned long mark, markMax = 0, markMaxLesser = 0; // Benchmark评分，注意返回第二峰值分数 
	static unsigned long markMaxDuration = 0; // 连续50次计数没有更新markMax就结束Benchmark模式 
	
	static double lastRecordedTime = 0.0; // 游戏内置计时器
	static double lastDisplayTime = 0.0; // 控制一秒输出一次
	
	if (general.resetFpsCounter){
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
    
    // 自己动手模式不计算且不输出 
    if (general.model == 0)return 0;
    
    if (lastRecordedTime - (double)fpsRate > lastDisplayTime){
    	fraps = framesElapsed * 1000.0 / (lastRecordedTime - lastDisplayTime);
		mark = (unsigned long)(totalFramesElapsed * 100000.0 / (lastRecordedTime - general.startTime));
    	
    	SetConsoleTextAttribute(g_hConsoleOutput, 0x0B);
    	if (general.benchmark)
    	{
			gotoxyWithFullwidth(32, 20);
    		printf("%6u%4s", mark, "");
    		if (mark > markMax)
			{ 
				markMaxDuration = 0;
				markMaxLesser = markMax;
				markMax = mark; 
				gotoxyWithFullwidth(32, 21);
    			printf("%6u%4s", markMaxLesser, "");
    		}
    		else if (mark > markMaxLesser){
    			markMaxDuration++;
    			markMaxLesser = mark;
				gotoxyWithFullwidth(32, 21);
    			printf("%6u%4s", markMaxLesser, "");
    		}
    		else
    		{
    			markMaxDuration++;
    			if (markMaxDuration > 15)
    			{
    				return -(signed long)markMaxLesser; // 终止Benchmark模式的信号 
    			}
    		}
    	}
    	else if (fraps > 999.48)
    	{
    		SetConsoleTextAttribute(g_hConsoleOutput, 0x0C);
			gotoxyWithFullwidth(32, 21);
    		printf("Maxed Out!");
    	}
		else
		{
			gotoxyWithFullwidth(32, 21);
    		printf("%6.2lf fps", fraps);
    	}
    	lastDisplayTime = lastRecordedTime;
    	framesElapsed = 0;
    }
    return markMax;
}

inline void pause(void)
{
	while(_kbhit())_getch();
	while(!_kbhit());
	while(_kbhit())_getch();
}

inline void clrscr(void){
	static DWORD clsCount; 
	static COORD startPos = {0, 0};
	static CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
	static DWORD consoleScreenBufferSize;
	
	// 计算屏幕缓冲区大小 
	GetConsoleScreenBufferInfo(g_hConsoleOutput,&consoleScreenBufferInfo);
	consoleScreenBufferSize = consoleScreenBufferInfo.dwSize.X * consoleScreenBufferInfo.dwSize.Y;
	
	// 清屏 
	FillConsoleOutputCharacter(g_hConsoleOutput,0x20,consoleScreenBufferSize,startPos,&clsCount);
	FillConsoleOutputAttribute(g_hConsoleOutput,0x0F,consoleScreenBufferSize,startPos,&clsCount);
	SetConsoleCursorPosition(g_hConsoleOutput,startPos);
}


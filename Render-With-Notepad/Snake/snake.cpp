//Attempts to play snake with notepad
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <random>
#include <time.h>
#include "notepad_frontend.h"

const int SCREEN_CHARS_WIDE = 131;
const int SCREEN_CHARS_TALL = 30;
const int SCREEN_NUM_CHARS = SCREEN_CHARS_WIDE * SCREEN_CHARS_TALL;
const int WINDOW_WIDTH_PIXELS = 1366;
const int WINDOW_HEIGHT_PIXELS = 768;

const int KEYCODE_W = 87;
const int KEYCODE_A = 65;
const int KEYCODE_S = 83;
const int KEYCODE_D = 68;

const char SNAKE_BODY = 'O';
const char SNAKE_HEAD = '@';
const char TARGET = 'X';

typedef struct
{
	int x;
	int y;
}Point;

enum GameMode
{
	PLAYING,
	GAMEOVER
};

typedef struct
{
	Point worldSize;
	Point velocity;
	GameMode currentMode;
	time_t gameOverTime;
	Point targetPos;
	int score;
	CRITICAL_SECTION velocityCritical;

	//stupid simple dequeue for snake segments
	//doesn't do error handling because yolo
	Point snakeSegments[SCREEN_NUM_CHARS];
	int back = -1;
	int front = -1;
}SnakeGame;

SnakeGame game;

void popSegment()
{
	game.back = (game.back - 1 + SCREEN_NUM_CHARS) % SCREEN_NUM_CHARS;
}

void queueSegmentBack(Point p)
{
	if (game.back == game.front && game.front == -1)
	{
		game.back = game.front = 0;
		game.snakeSegments[0] = p;
	}
	else
	{
		game.back = (game.back + 1) % SCREEN_NUM_CHARS;
		game.snakeSegments[game.back] = p;
	}
}

void queueSegmentFront(Point p)
{
	if (game.back == game.front && game.front == -1)
	{
		game.back = game.front = 0;
		game.snakeSegments[0] = p;
	}
	else
	{
		game.front = (game.front - 1 + SCREEN_NUM_CHARS) % SCREEN_NUM_CHARS;
		game.snakeSegments[game.front] = p;
	}
}

void handleInput(int code)
{
	EnterCriticalSection(&game.velocityCritical);

	switch (code)
	{
	case KEYCODE_W: if (game.velocity.y != 1) game.velocity = { 0,-1 }; break;
	case KEYCODE_A: if (game.velocity.x != 1) game.velocity = { -1,0 }; break;
	case KEYCODE_S: if (game.velocity.y != -1) game.velocity = { 0,1 }; break;
	case KEYCODE_D: if (game.velocity.x != -1) game.velocity = { 1,0 }; break;
	}

	LeaveCriticalSection(&game.velocityCritical);
}

void newTarget()
{
	Point candidate = { rand() % SCREEN_CHARS_WIDE, rand() % SCREEN_CHARS_TALL };
	char c = getChar(candidate.x, candidate.y);

	int iterations = 1;
	while (c == SNAKE_BODY || c == SNAKE_HEAD)
	{
		candidate = { rand() % SCREEN_CHARS_WIDE, rand() % SCREEN_CHARS_TALL };
		iterations++;
		if (iterations > 20)
		{
			//if we're unlucky, just brute force the target
			//this would be terrible if we weren't drawing to a tiny char array

			for (int i = 0; i < SCREEN_CHARS_TALL; ++i)
			{
				for (int j = 0; j < SCREEN_CHARS_WIDE; ++j)
				{
					char t = getChar(j, i);
					if (c != SNAKE_BODY && c != SNAKE_HEAD)
					{
						game.targetPos = { j,i };
						return;
					}
				}
			}
		}
	}

	game.targetPos = candidate;
}

bool tick()
{
	if (game.currentMode == PLAYING)
	{
		Point newHead = { 0 };

		EnterCriticalSection(&game.velocityCritical);

		newHead = game.snakeSegments[game.front];
		newHead.x += game.velocity.x;
		newHead.y += game.velocity.y;
		printf("%i %i vel %i %i\n", newHead.x, newHead.y, game.velocity.x, game.velocity.y);
		LeaveCriticalSection(&game.velocityCritical);
		bool gameOver = false;

		if (newHead.x == game.targetPos.x && newHead.y == game.targetPos.y)
		{
			newTarget();
			queueSegmentBack(newHead);
			queueSegmentBack(newHead);
			queueSegmentBack(newHead);
		}
		else
		{
			if (newHead.x < 0 || newHead.y < 0 || newHead.x >= SCREEN_CHARS_WIDE || newHead.y >= SCREEN_CHARS_TALL)
			{
				gameOver = true;
			}
			else
			{
				int iter = game.front;
				while (iter != game.back)
				{
					Point seg = game.snakeSegments[iter];
					if (newHead.x == seg.x && newHead.y == seg.y)
					{
						gameOver = true;
						break;
					}
					iter = (iter + 1) % SCREEN_NUM_CHARS;
				}

				if (game.front != game.back)
				{
					Point seg = game.snakeSegments[game.back];
					if (newHead.x == seg.x && newHead.y == seg.y)
					{
						gameOver = true;
					}
				}
			}

		}

		if (gameOver)
		{
			time(&game.gameOverTime);
			game.currentMode = GAMEOVER;
		}
		else
		{
			popSegment();
			queueSegmentFront(newHead);
		}
	}
	else
	{
		time_t curTime;
		time(&curTime);

		if (curTime - game.gameOverTime > 3)
		{
			return false;
		}
	}

	return true;
}

void drawGame()
{
	drawChar(game.targetPos.x, game.targetPos.y, TARGET);

	int iter = game.front;
	Point seg = game.snakeSegments[iter];
	drawChar(seg.x, seg.y, SNAKE_HEAD);
	iter = (iter + 1) % SCREEN_NUM_CHARS;

	while (iter != game.back)
	{
		seg = game.snakeSegments[iter];
		drawChar(seg.x, seg.y, SNAKE_BODY);
		iter = (iter + 1) % SCREEN_NUM_CHARS;
	}

	seg = game.snakeSegments[game.back];
	drawChar(seg.x, seg.y, SNAKE_BODY);

}

int main(int argc, const char** argv)
{
	//initial setup
	initializeNotepadFrontend(argv[1], argv[2], WINDOW_WIDTH_PIXELS, WINDOW_HEIGHT_PIXELS, SCREEN_CHARS_WIDE, SCREEN_NUM_CHARS);
	setKeydownFunc(handleInput);
	InitializeCriticalSection(&game.velocityCritical);

	queueSegmentFront({ SCREEN_CHARS_WIDE / 2, SCREEN_CHARS_TALL / 2 });
	queueSegmentBack({ SCREEN_CHARS_WIDE / 2, SCREEN_CHARS_TALL / 2 });
	queueSegmentBack({ SCREEN_CHARS_WIDE / 2, SCREEN_CHARS_TALL / 2 });
	game.velocity.x = 1;
	newTarget();

	while (1)
	{
		clearScreen();
		if (!tick())
		{
			break;
		}
		drawGame();
		swapBuffersAndRedraw();
		Sleep(33);
	}

	shutdownNotepadFrontend();

	return 0;
}
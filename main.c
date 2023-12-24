/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>

#include "tamaemu.h"
#include "lcd.h"
#include "benevolentai.h"
#include "udp.h"

#include <stdbool.h>
#include <SDL.h>
#include <SDL2_framerate.h>
#include <SDL2_gfxPrimitives.h>

#include "grid.h"


// Define screen dimensions
#define SCREEN_WIDTH    480
#define SCREEN_HEIGHT   480
#define ICONSIZE   80

Tamagotchi *tama;
bool stopgame=false;

void sigintHdlr(int sig)  {
	if (tama->cpu->Trace) {
		printf("\n");
	
	}
	tama->cpu->Trace=1;
    	stopgame = true;
}


int getKey() {
	char buff[100];
	fd_set fds;
	struct timeval tv;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	tv.tv_sec=0;
	tv.tv_usec=0;
	select(1, &fds, NULL, NULL, &tv);
	if (FD_ISSET(0, &fds)) {
		fgets(buff, 99, stdin);
		return buff[0];
	} else {
		return 0;
	}
}



void updategrid(Grid *grid, Display *lcd) {
    	int i;
    int x,y;
	//const char *grays[4]={COLOR_WHITE,COLOR_GRAY,COLOR_DARK_GRAY,COLOR_BLACK};
	for (y=0; y<32; y++) {
		for (x=0; x<48; x++) {
             switch (lcd->p[y][x]&3) {
                case 0: grid->cells[x][y].rectColor = COLOR_WHITE; break;
                case 1: grid->cells[x][y].rectColor = COLOR_GRAY; break;
                case 2: grid->cells[x][y].rectColor = COLOR_DARK_GRAY; break;
                case 3:  grid->cells[x][y].rectColor = COLOR_BLACK; break;
                default: break;
             }
		}
	}
  //  grid->icons = lcd->icons;

i=lcd->icons;
	for (x=0; x<10; x++) {
		if (i&1) grid->icons = x;
		i>>=1;
	}
      char* str = malloc( 2 + 1 );
       snprintf( str, 2 + 1, "%d", grid->icons );

   grid->text2 = str;
}


#define FPS 15

int main(int argc, char **argv) {
	unsigned char **rom;
	long us;
	int k, i;
	int speedup=0;
	int stopDisplay=0;
	int aiEnabled=0;
    int silent=0;
	int t=0;
	char *eeprom="tama.eep";
	char *host="127.0.0.1";
	char *romdir="rom";
	struct timespec tstart, tend;
	uint8_t *dram;
	int sizex, sizey;
	Display display;
	int err=0;
    bool testbla = false;
    // Define screen dimensions
int w = SCREEN_WIDTH;
int h = SCREEN_HEIGHT;
int iconsize = ICONSIZE;

	srand(time(NULL));
	for (i=1; i<argc; i++) {
		if (strcmp(argv[i],"-h")==0 && argc>i+1) {
			i++;
			host=argv[i];
		} else if (strcmp(argv[i],"-e")==0 && argc>i+1) {
			i++;
			eeprom=argv[i];
		} else if (strcmp(argv[i],"-r")==0 && argc>i+1) {
			i++;
			romdir=argv[i];
		} else if (strcmp(argv[i], "-n")==0) {
			aiEnabled=0;
        } else if (strcmp(argv[i], "-s")==0) {
            silent=1;
		} else {
			printf("Unrecognized option - %s\n", argv[i]);
			err=1;
			break;
		}
	}

	if (err) {
		printf("Usage: %s [options]\n", argv[0]);
		printf("-h host - change tamaserver host address (def 127.0.0.1)\n");
		printf("-e eeprom.eep - change eeprom file (def tama.eep)\n");
		printf("-r rom/ - change rom dir\n");
		printf("-n - disable AI\n");
        printf("-s - disable console noise\n");
		exit(0);
	}


//SDL
  // Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL could not be initialized!\n"
                        "SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

#if defined linux && SDL_VERSION_ATLEAST(2, 0, 8)
    // Disable compositor bypass
    if(!SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0"))
    {
        printf("SDL can not disable compositor bypass!\n");
        return 0;
    }
#endif

    // Create window
    SDL_Window *window = SDL_CreateWindow("Basic C SDL game",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if(!window)
    {
        fprintf(stderr, "Window could not be created!\n"
                        "SDL_Error: %s\n", SDL_GetError());
    }
    else
    {
        // Create renderer
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
        if(!renderer)
        {
            fprintf(stderr, "Renderer could not be created!\n"
                            "SDL_Error: %s\n", SDL_GetError());
        }
        else
        {
            // Start the game


        // Init grid
    Grid grid = {0};

    // Set grid dimensions
    int margin = 1  ;
    //grid.rect.w = MIN(w - margin * 2, h - margin * 2);
    //grid.rect.h = grid.rect.w;

grid.rect.w = w;
grid.rect.h = h - iconsize;

    // Set grid backgroud
    grid.backgroundColor = COLOR_BLACK;

    // Set grid border thickness and color
    grid.border = 0;
    grid.borderColor = COLOR_GRAY;

    // Set number of cells
    grid.xCells = 48;
    grid.yCells = 32;

    // Set cells border thickness and color
    grid.cellsBorder = grid.border;
    grid.cellsBorderColor = grid.borderColor;


    // Ajust size and center
    Grid_ajustSize(&grid);
    Grid_alignCenter(&grid, w, h);

    if(!Grid_init(&grid))
    {
        fprintf(stderr, "Grid fail to initialize !\n");
        return false;
    }

    SDL_Texture* t_icon1 = grid_loadimg(renderer, "img/1.bmp");
    SDL_Texture* t_icon2 = grid_loadimg(renderer, "img/2.bmp");

SDL_Rect icon1;
icon1.x = 10; icon1.y = 10; icon1.w = 64; icon1.h = 64;

SDL_Rect icon2;
icon2.x = 50; icon2.y = 10; icon2.w = 64; icon2.h = 64;

    // Initialize framerate manager : 30 FPS
    FPSmanager fpsmanager;
    SDL_initFramerate(&fpsmanager);
    SDL_setFramerate(&fpsmanager, 30);

    // Initialize start time (in ms)
    long long last = Utils_time();

    // Event loop exit flag
    bool quit = false;



	signal(SIGINT, sigintHdlr);
	rom=loadRoms(romdir);
	tama=tamaInit(rom, eeprom);
	dram=(uint8_t *)tama->dram;
	benevolentAiInit();
	udpInit(host);
	while(1) {


        SDL_Event e;
		clock_gettime(CLOCK_MONOTONIC, &tstart);
		tamaRun(tama, FCPU/FPS-1);
		sizex=tama->lcd.sizex;
		sizey=tama->lcd.sizey;
		lcdRender(dram, sizex, sizey, &display);
		udpTick();
		if (aiEnabled) {
			k=benevolentAiRun(&display, 1000/FPS);
		} else {
			k=0;
		}
		if (!speedup || (t&FPS)==0) {
          //  udpSendDisplay(&display);
            if (!silent) {
         //       lcdShow(&display);
                updategrid(&grid, &display);
                tamaDumpHw(tama->cpu);
                benevolentAiDump();
            }
		}
		if ((k&8)) {
			//If anything interesting happens, make a LCD dump.
			lcdDump(dram, sizex, sizey, "lcddump.lcd");
			if (stopDisplay) {
				tama->cpu->Trace=1;
				speedup=0;
			}
		}
		if (k&1) tamaPressBtn(tama, 0);
		if (k&2) tamaPressBtn(tama, 1);
		if (k&4) tamaPressBtn(tama, 2);
		clock_gettime(CLOCK_MONOTONIC, &tend);
		us=(tend.tv_nsec-tstart.tv_nsec)/1000;
		us+=(tend.tv_sec-tstart.tv_sec)*1000000L;
		us=(1000000L/FPS)-us;
		//printf("Time left in frame: %d us\n", us);
		if (!speedup && us>0) usleep(us);
	
        // Get available event
        while(SDL_PollEvent(&e))
        {
            // User requests quit
            if(e.type == SDL_QUIT)
            {
                stopgame = true;
                break;
            }
            else if(e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                stopgame = true;
                    break;

                case SDLK_1:
                 grid.text = "Button 0";
                tamaPressBtn(tama, 0);
                    break;
                case SDLK_2:
                 grid.text = "Button 1";
                tamaPressBtn(tama, 1);
                    break;
                case SDLK_3:
                 grid.text = "Button 2";
                tamaPressBtn(tama, 2);
                    break;
                case SDLK_s:
                speedup=!speedup;
                    break;
                case SDLK_d:
                 testbla = testbla ^ 1;
                    break;
        }
            }
        }




        // Set background color
        Utils_setBackgroundColor(renderer, COLOR_BLACK);

        // Render grid
       Grid_render(&grid, renderer);

grid_rendericons(renderer, t_icon1, icon1);

if (testbla) grid_rendericons(renderer, t_icon2, icon2);

        // Update screen
        SDL_RenderPresent(renderer);

        // Delay
        SDL_framerateDelay(&fpsmanager);
        	
            
        t++;


if (stopgame) break;

	}

tamaDeinit(tama);

 // Show message
        stringRGBA(renderer, grid.rect.x + grid.xCells, grid.rect.y - 20,
                   "exit",
                   COLOR_LIGHT_GRAY.r, COLOR_LIGHT_GRAY.g, COLOR_LIGHT_GRAY.b, COLOR_LIGHT_GRAY.a);

        // Update screen
        SDL_RenderPresent(renderer);

// Destroy renderer
SDL_DestroyRenderer(renderer);
        }

// Destroy window
SDL_DestroyWindow(window);
    }

    // Quit SDL
    SDL_Quit();

    return 0;
}
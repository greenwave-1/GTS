//
// Created on 12/21/25.
//

#include "util/args.h"

#include <getopt.h>
#include <strings.h>

#include "util/datetime.h"
#include "menu.h"

static struct option launchFlags[] =
		{
				{"date", required_argument, 0, 'd'},
				{"menu", required_argument, 0, 'm'},
				{ 0, 0, 0, 0 }
		};

// mostly based on the gnu example for long args:
// https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Option-Example.html

// NOTE: wiiload doesn't send the working directory by default when passing args
// gts isn't doing anything with that so adding another 'arg' between the .dol and the proper
// arguments works
void handleArgs(int argc, char **argv) {
	int optionIndex = 0;
	int c;
	
	// loop until we check all the flags
	while (true) {
		c = getopt_long(argc, argv, "d:m:", launchFlags, &optionIndex);
		
		// leave if we've exhausted all of our args
		if (c == -1) {
			break;
		}
		
		switch (c) {
			case 0:
				break;
			case 'd':
				#ifndef NO_DATE_CHECK
				if (optarg) {
					if (strcasecmp(optarg, "pm") == 0) {
						forceDate(DATE_PM);
					}
					if (strcasecmp(optarg, "cmas") == 0) {
						forceDate(DATE_CMAS);
					}
					if (strcasecmp(optarg, "nice") == 0) {
						forceDate(DATE_NICE);
					}
					if (strcasecmp(optarg, "none") == 0) {
						forceDate(DATE_NONE);
					}
				}
				#endif
				break;
			case 'm':
				if (optarg) {
					// no reason to force main menu, its the one that opens by default...
					if (strcasecmp(optarg, "controllertest") == 0) {
						menu_setCurrentMenu(CONTROLLER_TEST);
					}
					if (strcasecmp(optarg, "oscilloscope") == 0) {
						menu_setCurrentMenu(WAVEFORM);
					}
					if (strcasecmp(optarg, "2dplot") == 0) {
						menu_setCurrentMenu(PLOT_2D);
					}
					if (strcasecmp(optarg, "buttonplot") == 0) {
						menu_setCurrentMenu(PLOT_BUTTON);
					}
					// no reason to force file export since on launch there won't be data to export...
					if (strcasecmp(optarg, "coordview") == 0) {
						menu_setCurrentMenu(COORD_MAP);
					}
					if (strcasecmp(optarg, "continuous") == 0) {
						menu_setCurrentMenu(CONTINUOUS_WAVEFORM);
					}
					if (strcasecmp(optarg, "trigger") == 0) {
						menu_setCurrentMenu(TRIGGER_WAVEFORM);
					}
					if (strcasecmp(optarg, "gate") == 0) {
						menu_setCurrentMenu(GATE_MEASURE);
					}
					if (strcasecmp(optarg, "thanks") == 0) {
						menu_setCurrentMenu(THANKS_PAGE);
					}
				}
				break;
			case '?':
				break;
			default:
				break;
		}
	}
}

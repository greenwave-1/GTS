//
// Created on 8/26/25.
//

#ifndef GTS_CONTROLLERTEST_H
#define GTS_CONTROLLERTEST_H

enum CONTROLLER_TEST_MENU_STATE { CONT_TEST_SETUP, CONT_TEST_POST_SETUP };

void menu_controllerTest(void *currXfb);
void menu_controllerTestEnd();

#endif //GTS_CONTROLLERTEST_H

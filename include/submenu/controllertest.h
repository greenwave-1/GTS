//
// Created on 8/26/25.
//

// Controller Test submenu
// basic visual button and stick test
// also shows coordinates (raw and melee converted), and origin values

#ifndef GTS_CONTROLLERTEST_H
#define GTS_CONTROLLERTEST_H

enum CONTROLLER_TEST_MENU_STATE { CONT_TEST_SETUP, CONT_TEST_POST_SETUP };

void menu_controllerTest();
void menu_controllerTestEnd();

void menu_controllerTestToggleRumbleSecret(bool state);

#endif //GTS_CONTROLLERTEST_H

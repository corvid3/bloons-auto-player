#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <errno.h>
#include <pthread.h>
#include <sigsegv.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static Display *G_display;
static Window G_window;
static Window G_btd6window;
static int G_display_width;
static int G_display_height;

typedef struct {
    int r, g, b;
} Vec3;

void move_cursor(int x, int y) {
    float rx = (float)x / 1920;
    float ry = (float)y / 1080;
    XWarpPointer(G_display, None, G_window, 0, 0, 0, 0, rx * G_display_width,
                 ry * G_display_height);
    XFlush(G_display);
    usleep(250000);
}

static inline void get_curs_loc(int *x, int *y) {
    int ignore;
    unsigned mask_return;
    Window root_return, child_return;
    XQueryPointer(G_display, G_window, &root_return, &child_return, x, y,
                  &ignore, &ignore, &mask_return);
}

static inline void send_click(void) {
    XTestFakeButtonEvent(G_display, 1, True, CurrentTime);
    XFlush(G_display);
    XTestFakeButtonEvent(G_display, 1, False, CurrentTime);
    XFlush(G_display);

    // pretty much every click in this game has a second-long animation
    usleep(500000);
    // XButtonEvent event = {0};
    // event.button = Button1;
    // event.type = ButtonPress;
    // XSendEvent(G_display, G_window, True, ButtonPressMask, (XEvent*)&event);
}

static inline void send_key(unsigned int keysym) {
    int k = XKeysymToKeycode(G_display, keysym);
    XEvent event = {0};
    event.xkey.keycode = k;
    event.type = KeyPress;
    XSendEvent(G_display, G_btd6window, True, (KeyPressMask), &event);
    XFlush(G_display);
    usleep(180000);
    event.type = KeyRelease;
    XSendEvent(G_display, G_btd6window, False, (KeyReleaseMask), &event);
    XFlush(G_display);

    // wait just to make sure stuff is in order
    usleep(180000);
}

Vec3 get_color_at(int x, int y) {
    float rx = (float)x / 1920;
    float ry = (float)y / 1080;
    XImage *image = XGetImage(
        G_display, XRootWindow(G_display, DefaultScreen(G_display)),
        rx * G_display_width, ry * G_display_height, 1, 1, AllPlanes, XYPixmap);
    XColor color;
    color.pixel = XGetPixel(image, 0, 0);
    XFree(image);
    XQueryColor(G_display,
                XDefaultColormap(G_display, XDefaultScreen(G_display)), &color);

    return (Vec3){
        .r = color.red / 256,
        .g = color.green / 256,
        .b = color.blue / 256,
    };
}

Vec3 get_color_at_cursor(void) {
    // save cursor position, so that it doesn't overlap where we want to be
    int curs_y, curs_x;
    get_curs_loc(&curs_x, &curs_y);
    move_cursor(0, 0);

    Vec3 out = get_color_at(curs_x, curs_y);

    move_cursor(curs_x, curs_y);
    return out;
}

// send key events according to upgrade path
void do_upgrades(int a, int b, int c) {
    for (int i = 0; i < a; i++)
        send_key(XK_comma);

    for (int i = 0; i < b; i++)
        send_key(XK_period);

    for (int i = 0; i < c; i++)
        send_key(XK_slash);
}

void *kill_thread(void *args) {
    int *pid_heap = (int *)args;
    int master_pid = *pid_heap;

    printf("test");
    fflush(stdout);
    Display *display = XOpenDisplay(0);
    if (display == NULL) {
        printf("failed to open display\n");
        fflush(stdout);
        kill(master_pid, SIGKILL);
        exit(0);
    }

    XGrabKeyboard(display, XRootWindow(display, 0), True, GrabModeAsync,
                  GrabModeAsync, CurrentTime);

    while (True) {
        XEvent event;
        XNextEvent(display, &event);

        fflush(stdout);
        if (event.type == KeyPress) {
            int keysym = XkbKeycodeToKeysym(display, event.xkey.keycode, 0, 0);
            if (keysym == XK_backslash) {
                printf("killing program\n");
                fflush(stdout);
                kill(master_pid, SIGKILL);
                XUngrabKeyboard(display, CurrentTime);
                exit(1);
            }
        }
    }

    return NULL;
}

int vec3_within_10(Vec3 target, Vec3 got) {
    if (got.r > target.r - 10 && got.r < target.r + 10)
        if (got.g > target.g - 10 && got.g < target.g + 10)
            if (got.b > target.b - 10 && got.b < target.b + 10)
                return True;
    return False;
}

const static Vec3 HOME_BUTTON_BLUE_TARGET = {
    .r = 0,
    .g = 0xc8,
    .b = 0xff,
};

const static Vec3 STAR_BUTTON_YELLOW = {0xff, 0xdb, 0};

const static Vec3 NEXT_BUTTON_GREEN_TARGET = {0x43, 0xd8, 0};
const static Vec3 EXISTS_BUTTON_TARGET = {0x27, 0xd8, 0};

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("needs 1 arg, how many times to loop\n");
        exit(1);
    }

    errno = 0;
    int len = strtol(argv[1], NULL, 10);
    if (errno != 0) {
        printf("invalid input, should be a number for the no. of loops\n");
        exit(1);
    }

    G_display = XOpenDisplay(0);
    G_window = XRootWindow(G_display, 0);

    printf("You have 5 seconds to kill this program before it starts.\n"
           "Navigate to the BTD6 main menu within that timeframe.\n"
           "When you want to quit, press backslash.\n");
    printf("looping for %i times\n", len);
    G_display_height = XDisplayHeight(G_display, 0);
    G_display_width = XDisplayWidth(G_display, 0);

    usleep(5000000);

    int trash;
    XGetInputFocus(G_display, &G_btd6window, &trash);

    printf("started\n");
    fflush(stdout);

    int pid = getpid();
    int *pid_heap = malloc(sizeof(int));
    *pid_heap = pid;

    // spin up the kill thread
    pthread_t thread;
    pthread_create(&thread, NULL, kill_thread, pid_heap);
    pthread_detach(thread);

    for (int i = 0; i < len; i++) {
        // goes to the map and starts the game
        move_cursor(800, 900);
        send_click();
        move_cursor(1300, 950);
        send_click();
        move_cursor(1000, 600);
        send_click();
        // check if there is already a game, if so, set the 'game exists' to
        // true
        Vec3 game_exists_col = get_color_at(550, 1000);
        Bool game_exists =
            vec3_within_10(EXISTS_BUTTON_TARGET, game_exists_col);
        move_cursor(650, 400);
        send_click();
        move_cursor(1300, 500);
        send_click();

        if (game_exists) {
            move_cursor(1150, 725);
            send_click();
        }

        // it takes a while to load into the map, so wait 5 seconds
        usleep(5000000);

        // click the ok button
        move_cursor(875, 750);
        send_click();

        usleep(500000);

        // place towers and upgrade
        send_key(XK_K);
        move_cursor(100, 640);
        send_click();
        send_click();
        do_upgrades(2, 0, 2);

        send_key(XK_Z);
        move_cursor(100, 560);
        send_click();
        send_click();
        do_upgrades(0, 2, 4);

        send_key(XK_F);
        move_cursor(100, 500);
        send_click();
        send_click();
        do_upgrades(4, 2, 0);

        // clicks start
        send_key(XK_space);
        send_key(XK_space);

        // check every 5 seconds if the round has ended yet or got knowledge
        while (1) {
            usleep(5000000);

            // first, check if there is a knawledge
            Vec3 star_col = get_color_at(950, 400);
            if (vec3_within_10(STAR_BUTTON_YELLOW, star_col)) {
                // ok double check incase it was in the middle of an animation
                usleep(500000);
                Vec3 star_col = get_color_at(950, 400);
                if (!vec3_within_10(STAR_BUTTON_YELLOW, star_col))
                    continue;

                send_click();
                send_click();
                continue;
            }

            // check if there is a win
            Vec3 next_col = get_color_at(1050, 925);
            if (vec3_within_10(NEXT_BUTTON_GREEN_TARGET, next_col)) {
                // ok double check incase it was in the middle of an animation
                usleep(500000);
                Vec3 next_col = get_color_at(1050, 925);
                if (!vec3_within_10(NEXT_BUTTON_GREEN_TARGET, next_col))
                    continue;

                move_cursor(1050, 925);
                send_click();
                move_cursor(750, 875);
                send_click();
                break;
            }
        }

        // sleep another 3 seconds while the animation for the main menu loads
        usleep(3000000);
    }
    // finally, kill the thread when we're done as a safety measure. should
    // never get here in the finished product
    pthread_kill(thread, SIGKILL);
}

// XColor col = get_color_at_cursor();
// printf("%i %i %i\n", col.red / 256, col.green / 256, col.blue / 256);

/*

keys:
    - K: monkey village
    - Z: sniper
    - F: alchemist

positions:
    - knowledge star: 950 400
    - exit round NEXT button: 1050 925
    - exit round HOME button: 750 875
    - home screen PLAY button: 800 900
    - expert button: 1300 950
    - infernal map button: 1000 600
    - button for saved game: 550 1000
    - easy button: 650 400
    - deflation button: 1300 500
    - clear-saved-button: 1150 725
    - 'ok' button: 875 750
    - alchemist loc: 100 500
    - sniper loc: 100 560
    - village loc: 100 640
*/

#include <termios.h>
#include <unistd.h>

static struct termios oldtio;

void term_init() {
	tcgetattr(0, &oldtio);
}


void term_raw(int raw) {
	if (raw) {
		struct termios tio={0};
		tcgetattr(0, &tio);
		tio.c_lflag &= ~(ICANON);
//		cfmakeraw(&tio);
		tcsetattr(0, TCSANOW, &tio);
	} else {
		tcsetattr(0, TCSANOW, &oldtio);
	}
}
//Written by Brendan Sailer (bsailer1) and Brennen Hogan (bhogan1)

#include <ncurses.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define WIDTH 43
#define HEIGHT 21
#define PADLX 1
#define PADRX WIDTH - 2

int setup_host(int);
int setup_client(char*, int);
void send_int(int);
int recv_int();
void sendPaddle(int);
void sendBall(char*);
void* listenSock(void*);

// Global variables recording the state of the game
// Position of ball
int ballX, ballY;
// Movement of ball
int dx, dy;
// Position of paddles
int padLY, padRY;
// Player scores
int scoreL, scoreR;
// Current Round
int curr_round, num_rounds;
// Determines if the program is the host
int isHost = 0;
// Socket for communication
int new_s;

// ncurses window
WINDOW *win;

/* Draw the current game state to the screen
 * ballX: X position of the ball
 * ballY: Y position of the ball
 * padLY: Y position of the left paddle
 * padRY: Y position of the right paddle
 * scoreL: Score of the left player
 * scoreR: Score of the right player
 */
void draw(int ballX, int ballY, int padLY, int padRY, int scoreL, int scoreR) {
    // Center line
    int y;
    for(y = 1; y < HEIGHT-1; y++) {
        mvwaddch(win, y, WIDTH / 2, ACS_VLINE);
    }
    // Score
    mvwprintw(win, 1, WIDTH / 2 - 3, "%2d", scoreL);
    mvwprintw(win, 1, WIDTH / 2 + 2, "%d", scoreR);
    // Ball
    mvwaddch(win, ballY, ballX, ACS_BLOCK);
    // Left paddle
    for(y = 1; y < HEIGHT - 1; y++) {
        int ch = (y >= padLY - 2 && y <= padLY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, PADLX, ch);
    }
    // Right paddle
    for(y = 1; y < HEIGHT - 1; y++) {
        int ch = (y >= padRY - 2 && y <= padRY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, PADRX, ch);
    }
    // Print the virtual window (win) to the screen
    wrefresh(win);
    // Finally erase ball for next time (allows ball to move before next refresh)
    mvwaddch(win, ballY, ballX, ' ');
}

/* Return ball and paddles to starting positions
 * Horizontal direction of the ball is randomized
 */
void reset() {
    ballX = WIDTH / 2;
    padLY = padRY = ballY = HEIGHT / 2;
    // dx is randomly either -1 or 1
    dx = (rand() % 2) * 2 - 1;
    dy = 0;
    // Draw to reset everything visually
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
}

/* Display a message with a 3 second countdown
 * This method blocks for the duration of the countdown
 * message: The text to display during the countdown
 */
void countdown(const char *message1, const char *message2) {
    int h = 5;
    int w = strlen(message1) + 4;
    WINDOW *popup = newwin(h, w, (LINES - h) / 2, (COLS - w) / 2);
    box(popup, 0, 0);
    mvwprintw(popup, 1, 2, message1);
	mvwprintw(popup, 2, 2, message2);
    int countdown;
    for(countdown = 3; countdown > 0; countdown--) {
        mvwprintw(popup, 3, w / 2, "%d", countdown);
        wrefresh(popup);
        sleep(1);
    }

	/* Resets the scores after each round and incriments the round counter */
	if(scoreR == 2 || scoreL == 2){
		scoreR = 0;
		scoreL = 0;
		curr_round += 1;
	}
    
	wclear(popup);
    wrefresh(popup);
    delwin(popup);
    padLY = padRY = HEIGHT / 2; // Wipe out any input that accumulated during the delay
}

void game_over() {
    int h = 5;
    int w = 14;
    WINDOW *popup = newwin(h, w, (LINES - h) / 2, (COLS - w) / 2);
    box(popup, 0, 0);
	mvwprintw(popup, 2, 2, "Game Ended");
    
    wrefresh(popup);
    padLY = padRY = HEIGHT / 2; // Wipe out any input that accumulated during the delay
}

/* Perform periodic game functions:
 * 1. Move the ball
 * 2. Detect collisions
 * 3. Detect scored points and react accordingly
 * 4. Draw updated game state to the screen
 */
void tock() {
    // Move the ball
    ballX += dx;
    ballY += dy;
    
    // Check for paddle collisions
	// hostSide is 1 when the ball is on the right hand side of the board
	int hostSide = (ballX < WIDTH / 2) ? 0 : 1;
    // padY is y value of closest paddle to ball
    int padY = (ballX < WIDTH / 2) ? padLY : padRY;
    // colX is x value of ball for a paddle collision
    int colX = (ballX < WIDTH / 2) ? PADLX + 1 : PADRX - 1;
    if(ballX == colX && abs(ballY - padY) <= 2) {
        // Collision detected!
        dx *= -1;
        // Determine bounce angle
        if(ballY < padY) dy = -1;
        else if(ballY > padY) dy = 1;
        else dy = 0;
		//Send the info based on the collision event
		if(isHost == hostSide){ //If the ball is on your side, make the call
			char ball_cords[BUFSIZ];

			sprintf(ball_cords, "B %d %d %d %d", ballX, ballY, dx, dy);
			sendBall(ball_cords);
		}
    }

    // Check for top/bottom boundary collisions
    if(ballY == 1) dy = 1;
    else if(ballY == HEIGHT - 2) dy = -1;
    
    // Score points
    if(ballX == 0) {
        scoreR = (scoreR + 1) % 100;
        reset();
		if(scoreR == 2){
			char message[BUFSIZ];
			sprintf(message, "Round %d", curr_round);
					countdown(message, "WIN -->");
		} else{
					countdown("SCORE -->", " ");
		}
    } else if(ballX == WIDTH - 1) {
        scoreL = (scoreL + 1) % 100;
        reset();
		if(scoreL == 2){
			char message[BUFSIZ];
			sprintf(message, "Round %d", curr_round);
					countdown(message, "<-- WIN");
		} else{
					countdown("<-- SCORE", " ");
		}
    }

    // Finally, redraw the current state
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
}

/* Listen to keyboard input
 * Updates global pad positions
 */
void *listenInput(void *args) {
	while(curr_round <= num_rounds) {
		if(isHost){ // Host uses arrow keys
			switch(getch()) {
				case KEY_UP: padRY--;
					sendPaddle(padRY);
					break;
				case KEY_DOWN: padRY++;
					sendPaddle(padRY);
					break;
				default: break;
			}
		} else { // Client uses w and s keys
			switch(getch()) {
				case 'w': padLY--;
					sendPaddle(padLY);
					break;
				case 's': padLY++;
					sendPaddle(padLY);
					break;
				default: break;
       		}
    	}
	}
	return NULL;
}

void initNcurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    refresh();
    win = newwin(HEIGHT, WIDTH, (LINES - HEIGHT) / 2, (COLS - WIDTH) / 2);
    box(win, 0, 0);
    mvwaddch(win, 0, WIDTH / 2, ACS_TTEE);
    mvwaddch(win, HEIGHT-1, WIDTH / 2, ACS_BTEE);
}

int main(int argc, char *argv[]) {
	// Process args
	// refresh is clock rate in microseconds
	// This corresponds to the movement speed of the ball
		
	char *host;
	int port;
	if(argc != 3){
		printf("Useage: ./netpong --host PORT\n");
		printf("Useage: ./netpong HOSTNAME PORT\n");
		exit(1);
	} else {
		if(strcmp(argv[1], "--host") == 0){
			isHost = 1;
		} else{
			host = argv[1];
		}
		port = atoi(argv[2]);
	}
	printf("isHost %d, port: %d, host: %s\n", isHost, port, host);

	int refresh;
	char difficulty[10]; 
	char rounds[10];
	if(isHost){ // Only let the host pick the difficulty and number of rounds
		printf("Please select the difficulty level (easy, medium or hard): ");
		scanf("%s", &difficulty);
		printf("Please enter the maximum number of rounds to play: ");
		scanf("%s", &rounds);
		num_rounds = atoi(rounds);

		while(num_rounds <= 0){
			printf("Please enter the maximum number of rounds to play (must be greater than 0): ");
			scanf("%s", &rounds);
			num_rounds = atoi(rounds);
		}

		// Setup host connection
		new_s = setup_host(port);
		if(strcmp(difficulty, "easy") == 0) refresh = 80000;
		else if(strcmp(difficulty, "medium") == 0) refresh = 40000;
		else if(strcmp(difficulty, "hard") == 0) refresh = 20000;
		else refresh = 80000;
	
		// Send the refresh and the number of rounds to the client
		send_int(refresh);
		int ack = recv_int();
		send_int(num_rounds);
	} else {
		// Connect to the host
		new_s = setup_client(host, port);

		// Receive the refresh and the number of rounds from the host
		refresh = recv_int();
		send_int(0);
		num_rounds = recv_int();
	}

	curr_round = 1;

	// Set up ncurses environment
	initNcurses();

	// Set starting game state and display a countdown
	reset();
	countdown("Starting Game", " ");
    
	// Listen to keyboard input in a background thread
	pthread_t pth;
	pthread_create(&pth, NULL, listenInput, NULL);
	
	// Listen to socket in background
	pthread_t listener;
	pthread_create(&listener, NULL, listenSock, NULL);

	// Main game loop executes tock() method every REFRESH microseconds
	struct timeval tv;
	while(curr_round <= num_rounds) {
		gettimeofday(&tv,NULL);
		unsigned long before = 1000000 * tv.tv_sec + tv.tv_usec;
		tock(); // Update game state
		gettimeofday(&tv,NULL);
		unsigned long after = 1000000 * tv.tv_sec + tv.tv_usec;
		unsigned long toSleep = refresh - (after - before);
		// toSleep can sometimes be > refresh, e.g. countdown() is called during tock()
		// In that case it's MUCH bigger because of overflow!
		if(toSleep > refresh) toSleep = refresh;
		usleep(toSleep); // Sleep exactly as much as is necessary
	}

	//Game ended print
	game_over();
 
	// Clean up
	pthread_join(pth, NULL);
	pthread_join(listener, NULL);
	endwin();
	return 0;
}

int setup_host(int port){
	struct sockaddr_in sin, client_addr;
	char buf[BUFSIZ];
	int len, addr_len;
	int s, connected_s;

	/* build address data structure */
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

	/* setup passive open */
	if((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("simplex-talk: socket");
		exit(1);
	}

	// set socket option
	int opt = 1;
	if((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)& opt, sizeof(int)))<0){
		perror ("netpong: setscokt");
		exit(1);
	}

	if((bind(s, (struct sockaddr *)&sin, sizeof(sin))) < 0) {
		perror("netpong: bind"); exit(1);
	}
	if((listen(s, 2))<0){
		perror("netpong: listen"); exit(1);
	}

	/* wait for connection, then receive and print text */
	addr_len = sizeof (client_addr);
	if((connected_s = accept(s, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
		perror("netpong: accept");
		exit(1);
	}

	close(s);
	return connected_s;
}

int setup_client(char* host, int port){
	struct hostent *hp;
	struct sockaddr_in sin;
	char buf[BUFSIZ];
	char reply[BUFSIZ];
	int s;

	/* translate host name into peer's IP address */
	hp = gethostbyname(host);
	if (!hp) {
		fprintf(stderr, "netpong: unknown host: %s\n", host);
		exit(1);
	}
	
	/* build address data structure */
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = htons(port);

	/* active open */
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("netpong: socket");
		exit(1);
	}

	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		perror("netpong: connect");
		close(s);
		exit(1);
	}

	return s;
}

void send_int(int value){
	int formatted = htonl(value);
	if(send(new_s, &formatted, sizeof(formatted), 0)==-1){
		perror("netpong: error sending message");
		exit(1);
	}
}

int recv_int(){
	int length;
	int received_int;
	if((length=recv(new_s, &received_int, sizeof(received_int), 0)) < 0){
		perror("netpong: error receiving reply");
		exit(1);
	}
	return ntohl(received_int);
}

void sendPaddle(int paddleY){
	char buf[BUFSIZ];

	sprintf(buf, "P %d", paddleY);

	if(send(new_s, buf, strlen(buf)+1, 0)==-1){
		printf("Server response error");
	}
}

void sendBall(char* buf){
	if(send(new_s, buf, strlen(buf)+1, 0)==-1){
		printf("Server response error");
	}
}

void* listenSock(void* args){
	char buf[BUFSIZ];
	int len;

	while (1){
		if((len=recv(new_s, buf, sizeof(buf), 0))==-1){
			perror("Server Received Error!");
			exit(1);
		}
		//Check if the sent data was for the paddle or ball
		char *send_type = strtok(buf, " ");
		if(strcmp(send_type, "P") == 0){
			char *paddlePos = strtok(NULL, " ");
			if(isHost){ //Host receives info for the left paddle
				padLY = atoi(paddlePos);
			} else{ //P2 receives info for the right paddle
				padRY = atoi(paddlePos);
			}
		} else if(strcmp(send_type, "B") == 0){
			char *b_x = strtok(NULL, " ");
			ballX = atoi(b_x);
			char *b_y = strtok(NULL, " ");
			ballY = atoi(b_y);
			char *d_x = strtok(NULL, " ");
			dx = atoi(d_x);
			char *d_y = strtok(NULL, " ");
			dy = atoi(d_y);
		}

		bzero((char *) &buf, sizeof(buf));
	}

}

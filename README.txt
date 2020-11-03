The files included are: netpong.c, Makefile, and README.txt

To compile run: make

To run the host: ./netpong --host PORT
	Then, you should select a difficulty and then the number of rounds to play.
	If the difficulty is not easy, medium, or hard - it defaults to easy.
To run the client: ./netpong HOSTNAME PORT
	The game will start right away.

OVERVIEW:
We used an event-based system to update the position of the paddles and the ball.
As a result, we used TCP since the built-in delivery mechanism guarantees that the packet is delivered.
The Host uses the up and down arrow keys to control their paddle while the client uses the 'w' and 's' keys to move their paddle.
We implemented the logic when the ball is on your side of the court that you call the shots to resolve discrepancies.
To do this, we added an additional network thread which just listens for paddle movements and ball collisions.
When a player moves a paddle, this change is communicated over the network to the other user.
Likewise, when the ball is hit, that user communicates the new ball's position, dx, and dy.

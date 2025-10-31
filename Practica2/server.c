#include "server.h"

/** Shared array that contains all the games. */
tGame games[MAX_GAMES];
int gameIte = 0;

void initGame(tGame *game)
{

	// Init players' name
	memset(game->player1Name, 0, STRING_LENGTH);
	memset(game->player2Name, 0, STRING_LENGTH);

	// Alloc memory for the decks
	clearDeck(&(game->player1Deck));
	clearDeck(&(game->player2Deck));
	initDeck(&(game->gameDeck));

	// Bet and stack
	game->player1Bet = DEFAULT_BET;
	game->player2Bet = DEFAULT_BET;
	game->player1Stack = INITIAL_STACK;
	game->player2Stack = INITIAL_STACK;

	// Game status variables
	game->player1Stand = FALSE;
	game->player2Stand = FALSE;
	game->endOfGame = FALSE;
	game->status = gameEmpty;

	game->stacksUpdated = 0;
}

void resetRound(tGame *game)
{
	clearDeck(&game->player1Deck);
	clearDeck(&game->player2Deck);
	initDeck(&game->gameDeck);
	game->player1Stand = FALSE;
	game->player2Stand = FALSE;
	game->endOfGame = FALSE;
	game->stacksUpdated = 0;
}

void initServerStructures(struct soap *soap)
{

	if (DEBUG_SERVER)
		printf("Initializing structures...\n");

	// Init seed
	srand(time(NULL));

	// Init each game (alloc memory and init)
	for (int i = 0; i < MAX_GAMES; i++)
	{
		games[i].player1Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		games[i].player2Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		allocDeck(soap, &(games[i].player1Deck));
		allocDeck(soap, &(games[i].player2Deck));
		allocDeck(soap, &(games[i].gameDeck));
		pthread_mutex_init(&games[i].mutex_status, NULL);
		pthread_mutex_init(&games[i].mutex_game, NULL);
		pthread_cond_init(&games[i].cond_player_turn, NULL);
		pthread_cond_init(&games[i].hasta_que_llegue_el_segundo, NULL);
		initGame(&(games[i]));
	}
}

void initDeck(blackJackns__tDeck *deck)
{

	deck->__size = DECK_SIZE;

	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = i;
}

void clearDeck(blackJackns__tDeck *deck)
{

	// Set number of cards
	deck->__size = 0;
	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = UNSET_CARD;
}

tPlayer calculateNextPlayer(tPlayer currentPlayer)
{
	return ((currentPlayer == player1) ? player2 : player1);
}

unsigned int getRandomCard(blackJackns__tDeck *deck)
{

	unsigned int card, cardIndex, i;

	// Get a random card
	cardIndex = rand() % deck->__size;
	card = deck->cards[cardIndex];

	// Remove the gap
	for (i = cardIndex; i < deck->__size - 1; i++)
		deck->cards[i] = deck->cards[i + 1];

	// Update the number of cards in the deck
	deck->__size--;
	deck->cards[deck->__size] = UNSET_CARD;

	return card;
}

unsigned int calculatePoints(blackJackns__tDeck *deck)
{

	unsigned int points = 0;

	for (int i = 0; i < deck->__size; i++)
	{

		if (deck->cards[i] % SUIT_SIZE < 9)
			points += (deck->cards[i] % SUIT_SIZE) + 1;
		else
			points += FIGURE_VALUE;
	}

	return points;
}

void dealInitialCards(tGame *game)
{
	// Deal 2 cards to player 1
	unsigned int card1 = getRandomCard(&game->gameDeck);
	unsigned int card2 = getRandomCard(&game->gameDeck);
	game->player1Deck.cards[game->player1Deck.__size++] = card1;
	game->player1Deck.cards[game->player1Deck.__size++] = card2;

	// Deal 2 cards to player 2
	card1 = getRandomCard(&game->gameDeck);
	card2 = getRandomCard(&game->gameDeck);
	game->player2Deck.cards[game->player2Deck.__size++] = card1;
	game->player2Deck.cards[game->player2Deck.__size++] = card2;
}

void copyGameStatusStructure(blackJackns__tBlock *status, char *message, blackJackns__tDeck *newDeck, int newCode)
{
	// Copy the message
	memset((status->msgStruct).msg, 0, STRING_LENGTH);
	strcpy((status->msgStruct).msg, message);
	(status->msgStruct).__size = strlen((status->msgStruct).msg);

	// Handle deck copying safely
	if (newDeck != NULL && newDeck->__size > 0)
	{
		memcpy((status->deck).cards, newDeck->cards, DECK_SIZE * sizeof(unsigned int));
		(status->deck).__size = newDeck->__size;
	}
	else
	{
		// Don't set cards to NULL - keep the allocated array but set size to 0
		(status->deck).__size = 0;
		// Clear the deck to avoid sending garbage data
		for (int i = 0; i < DECK_SIZE; i++)
		{
			(status->deck).cards[i] = UNSET_CARD;
		}
	}

	// Set the new code
	status->code = newCode;
}

/**
 * Determine game result when game ends
 */
void determineGameResult(tGame *game, int isPlayer1, char *msg, int *resultCode)
{
	unsigned int p1Points = calculatePoints(&game->player1Deck);
	unsigned int p2Points = calculatePoints(&game->player2Deck);

	printf("Determinando resultado\n");
	if (isPlayer1)
	{
		if (p1Points > GOAL_GAME)
		{
			strcpy(msg, "You busted! You lose!");
			*resultCode = GAME_LOSE;
		}
		else if (p2Points > GOAL_GAME)
		{
			strcpy(msg, "Opponent busted! You win!");
			*resultCode = GAME_WIN;
		}
		else if (p1Points > p2Points)
		{
			strcpy(msg, "You have more points! You win!");
			*resultCode = GAME_WIN;
		}
		else if (p2Points > p1Points)
		{
			strcpy(msg, "Opponent has more points! You lose!");
			*resultCode = GAME_LOSE;
		}
		else
		{
			strcpy(msg, "It's a tie!");
			*resultCode = GAME_TIE;
		}
	}
	else // Player 2
	{
		if (p2Points > GOAL_GAME)
		{
			strcpy(msg, "You busted! You lose!");
			*resultCode = GAME_LOSE;
		}
		else if (p1Points > GOAL_GAME)
		{
			strcpy(msg, "Opponent busted! You win!");
			*resultCode = GAME_WIN;
		}
		else if (p2Points > p1Points)
		{
			strcpy(msg, "You have more points! You win!");
			*resultCode = GAME_WIN;
		}
		else if (p1Points > p2Points)
		{
			strcpy(msg, "Opponent has more points! You lose!");
			*resultCode = GAME_LOSE;
		}
		else
		{
			strcpy(msg, "It's a tie!");
			*resultCode = GAME_TIE;
		}
	}
}

int checkGameEnd(tGame *game)
{
	unsigned int p1Points = calculatePoints(&game->player1Deck);
	unsigned int p2Points = calculatePoints(&game->player2Deck);

	printf("Checking game end\n");
	// Si ambos jugadores se han plantado
	if (game->player1Stand && game->player2Stand)
	{
		game->endOfGame = TRUE;
		return TRUE;
	}

	// Si un jugador se ha pasado de 21
	if (p1Points > GOAL_GAME || p2Points > GOAL_GAME)
	{
		game->endOfGame = TRUE;
		return TRUE;
	}

	return FALSE;
}

/* ===== WEB SERVICE OPERATIONS ===== */

/**
 * Get current game status for a player
 */
int blackJackns__getStatus(struct soap *soap,
						   blackJackns__tMessage playerName,
						   int gameId,
						   blackJackns__tBlock *result)
{
	allocClearBlock(soap, result);
	playerName.msg[playerName.__size] = 0;

	// Validate game ID
	if (gameId < 0 || gameId >= MAX_GAMES)
	{
		copyGameStatusStructure(result, "Invalid game ID", NULL, ERROR_SERVER_FULL);
		return SOAP_OK;
	}

	tGame *game = &games[gameId];

	// Verify player belongs to this game
	pthread_mutex_lock(&game->mutex_status);
	int isPlayer1 = strcmp(playerName.msg, game->player1Name) == 0;
	int isPlayer2 = strcmp(playerName.msg, game->player2Name) == 0;

	if (!isPlayer1 && !isPlayer2)
	{
		pthread_mutex_unlock(&game->mutex_status);
		copyGameStatusStructure(result, "Player not found", NULL, ERROR_PLAYER_NOT_FOUND);
		return SOAP_OK;
	}
	while (game->status == gameWaitingPlayer)
	{
		pthread_cond_wait(&game->hasta_que_llegue_el_segundo, &game->mutex_status);
	}
	pthread_mutex_unlock(&game->mutex_status);

	pthread_mutex_lock(&game->mutex_game);

	if (game->endOfGame || (game->player1Stand && game->player2Stand))
	{
		int resultCode;
		char msg[STRING_LENGTH];
		// Protección extra para que solo un hilo haga la actualización
		if (!game->stacksUpdated)
		{
			game->stacksUpdated = 1;

			// Determinar resultado
			determineGameResult(game, isPlayer1, msg, &resultCode);

			// ===== Actualización de stacks segura =====
			if (resultCode == GAME_WIN)
			{
				if (isPlayer1)
				{
					game->player1Stack += game->player2Bet;
					game->player2Stack = (game->player2Stack < game->player2Bet) ? 0 : (game->player2Stack - game->player2Bet);
				}
				else
				{
					game->player2Stack += game->player1Bet;
					game->player1Stack = (game->player1Stack < game->player1Bet) ? 0 : (game->player1Stack - game->player1Bet);
				}
			}
			else if (resultCode == GAME_LOSE)
			{
				if (isPlayer1)
				{
					game->player1Stack = (game->player1Stack < game->player1Bet) ? 0 : (game->player1Stack - game->player1Bet);
					game->player2Stack += game->player1Bet;
				}
				else
				{
					game->player2Stack = (game->player2Stack < game->player2Bet) ? 0 : (game->player2Stack - game->player2Bet);
					game->player1Stack += game->player2Bet;
				}
			}
			// En caso de empate, no se modifican los stacks

			printf("Stacks actualizados -> P1: %u | P2: %u\n",
				   game->player1Stack, game->player2Stack);

			// ===== Verificación de fin de partida real =====
			if (game->player1Stack == 0 || game->player2Stack == 0)
			{
				game->endOfGame = TRUE;
				char endMsg[STRING_LENGTH];
				int finalCode = (isPlayer1) ? ((game->player1Stack == 0) ? GAME_LOSE : GAME_WIN) : ((game->player2Stack == 0) ? GAME_LOSE : GAME_WIN);

				snprintf(endMsg, STRING_LENGTH, "Game over! %s",
						 (finalCode == GAME_WIN) ? "You win!" : "You lose!");
				copyGameStatusStructure(result, endMsg, NULL, finalCode);
			}
			else
			{
				resetRound(game); // FIX: pass game, not &game
				dealInitialCards(game);
				game->status = gameReady;
				game->currentPlayer = (rand() % 2 == 0) ? player1 : player2;
				game->stacksUpdated = 0;

				char newRoundMsg[STRING_LENGTH];
				unsigned int myStack = isPlayer1 ? game->player1Stack : game->player2Stack;
				blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;

				// Include previous round result AND new round info
				snprintf(newRoundMsg, STRING_LENGTH, "%s Starting new round! Your stack: %u",
						 msg, myStack);

				if (game->currentPlayer == (isPlayer1 ? player1 : player2))
				{
					copyGameStatusStructure(result, newRoundMsg, myDeck, TURN_PLAY);
				}
				else
				{
					copyGameStatusStructure(result, newRoundMsg, myDeck, TURN_WAIT);
				}
			}
		}
		else
		{

			if (game->endOfGame)
			{
				char endMsg[STRING_LENGTH];
				int finalCode = (isPlayer1) ? ((game->player1Stack == 0) ? GAME_LOSE : GAME_WIN) : ((game->player2Stack == 0) ? GAME_LOSE : GAME_WIN);
				snprintf(endMsg, STRING_LENGTH, "Game over! %s",
						 (finalCode == GAME_WIN) ? "You win!" : "You lose!");
				copyGameStatusStructure(result, endMsg, NULL, finalCode);
			}
			else
			{
				char waitMsg[STRING_LENGTH];
				unsigned int myStack = isPlayer1 ? game->player1Stack : game->player2Stack;
				blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;

				snprintf(waitMsg, STRING_LENGTH, "Waiting for new round to start. Your stack: %u", myStack);
				copyGameStatusStructure(result, waitMsg, myDeck, TURN_WAIT);
			}
		}

		pthread_cond_broadcast(&game->cond_player_turn);
		pthread_mutex_unlock(&game->mutex_game);
		return SOAP_OK;
	}
	// Wait if it's not player's turn
	if ((isPlayer1 && game->currentPlayer != player1) ||
		(isPlayer2 && game->currentPlayer != player2))
	{
		pthread_cond_wait(&game->cond_player_turn, &game->mutex_game);

		char waitMsg[STRING_LENGTH];
		snprintf(waitMsg, STRING_LENGTH, "Your opponent's move...");
		blackJackns__tDeck *opponentDeck = isPlayer1 ? &game->player2Deck : &game->player1Deck;
		printf("Envio deck al jugador que espera en gameStatus\n");
		copyGameStatusStructure(result, waitMsg, opponentDeck, TURN_WAIT);
	}
	else
	{
		// Game is ongoing - handle player's turn
		blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;
		char msg[STRING_LENGTH];

		// Deal initial cards if needed

		unsigned int myPoints = calculatePoints(myDeck);
		unsigned int stack = isPlayer1 ? game->player1Stack : game->player2Stack;
		snprintf(msg, STRING_LENGTH, "Your turn. You have %u points. Your stack is %u", myPoints, stack);
		printf("Envio turn play en gameStatus\n");
		copyGameStatusStructure(result, msg, myDeck, TURN_PLAY);
	}

	pthread_mutex_unlock(&game->mutex_game);
	return SOAP_OK;
}

/**
 * Register a new player in a game
 */
int blackJackns__register(struct soap *soap, blackJackns__tMessage playerName, int *result)
{
	int gameIndex = -1;
	int found = 0, i = 0;

	playerName.msg[playerName.__size] = 0;

	if (DEBUG_SERVER)
		printf("[Register] Registering new player -> [%s]\n", playerName.msg);

	// Search for available game slot
	while (i < MAX_GAMES && found == 0)
	{
		pthread_mutex_lock(&games[i].mutex_status);

		if (games[i].status == gameEmpty || games[i].endOfGame)
		{
			strncpy(games[i].player1Name, playerName.msg, STRING_LENGTH - 1);
			games[i].player1Name[STRING_LENGTH - 1] = 0;
			games[i].status = gameWaitingPlayer;
			gameIndex = i;
			if (games[i].endOfGame)
			{
				games[i].endOfGame = 0;
				initGame(&games[i]);
			}

			pthread_mutex_unlock(&games[i].mutex_status);
			found = 1;
		}
		else if (games[i].status == gameWaitingPlayer)
		{
			// Join existing game
			if (strcmp(playerName.msg, games[i].player1Name) == 0)
			{
				pthread_mutex_unlock(&games[i].mutex_status);
				*result = ERROR_NAME_REPEATED;
				return SOAP_OK;
			}

			strncpy(games[i].player2Name, playerName.msg, STRING_LENGTH - 1);
			games[i].player2Name[STRING_LENGTH - 1] = 0;
			games[i].status = gameReady;
			gameIndex = i;

			// Notify first player that second player has arrived
			dealInitialCards(&games[i]);
			games[i].currentPlayer = (rand() % 2 == 0) ? player1 : player2;
			pthread_cond_broadcast(&games[i].hasta_que_llegue_el_segundo);
			pthread_cond_broadcast(&games[i].cond_player_turn);
			pthread_mutex_unlock(&games[i].mutex_status);
			found = 1;
		}
		else
		{
			pthread_mutex_unlock(&games[i].mutex_status);
		}
	}

	// Return result
	if (gameIndex == -1)
		*result = ERROR_SERVER_FULL;
	else
		*result = gameIndex;

	if (DEBUG_SERVER)
		printf("[Register] %s registered in game %d\n", playerName.msg, gameIndex);

	return SOAP_OK;
}

/**
 * Process a player's move (hit or stand)
 */
int blackJackns__playerMove(struct soap *soap,
							blackJackns__tMessage playerName,
							int gameId,
							unsigned int move,
							blackJackns__tBlock *result)
{
	allocClearBlock(soap, result);
	playerName.msg[playerName.__size] = '\0';

	// Validate game ID
	if (gameId < 0 || gameId >= MAX_GAMES)
	{
		copyGameStatusStructure(result, "Invalid game ID", NULL, ERROR_SERVER_FULL);
		return SOAP_OK;
	}

	tGame *game = &games[gameId];

	// Verify player belongs to game
	pthread_mutex_lock(&game->mutex_status);
	int isPlayer1 = strcmp(playerName.msg, game->player1Name) == 0;
	int isPlayer2 = strcmp(playerName.msg, game->player2Name) == 0;

	if (!isPlayer1 && !isPlayer2)
	{
		copyGameStatusStructure(result, "Player not found", NULL, ERROR_PLAYER_NOT_FOUND);
		return SOAP_OK;
	}
	pthread_mutex_unlock(&game->mutex_status);

	pthread_mutex_lock(&game->mutex_game);

	blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;
	unsigned int myPoints = calculatePoints(myDeck);

	// Process player's move
	if (move == PLAYER_HIT_CARD)
	{
		unsigned int card = getRandomCard(&game->gameDeck);
		myDeck->cards[myDeck->__size++] = card;
		myPoints = calculatePoints(myDeck);

		char msg[STRING_LENGTH];
		snprintf(msg, STRING_LENGTH, "You drew a card. Now you have %u points.", myPoints);

		// Check if player busted
		if (myPoints > GOAL_GAME)
		{
			printf("Busted\n");
			game->endOfGame = TRUE;
		}
		copyGameStatusStructure(result, msg, myDeck, TURN_PLAY);
	}
	else if (move == PLAYER_STAND)
	{
		if (isPlayer1)
			game->player1Stand = TRUE;
		else
			game->player2Stand = TRUE;

		printf("Player stand\n");
		char msg[STRING_LENGTH];
		snprintf(msg, STRING_LENGTH, "You stand with %u points.", myPoints);
		game->currentPlayer = calculateNextPlayer(game->currentPlayer);
		copyGameStatusStructure(result, msg, myDeck, TURN_WAIT);
	}
	else
	{
		copyGameStatusStructure(result, "Invalid move", NULL, TURN_PLAY);
		pthread_cond_broadcast(&game->cond_player_turn);
		pthread_mutex_unlock(&game->mutex_game);
		return SOAP_OK;
	}

	// Notify other player
	pthread_cond_broadcast(&game->cond_player_turn);
	pthread_mutex_unlock(&game->mutex_game);

	return SOAP_OK;
}

// Agregar función de limpieza al finalizar el servidor
void cleanupServer()
{
	for (int i = 0; i < MAX_GAMES; i++)
	{
		pthread_mutex_destroy(&games[i].mutex_status);
		pthread_mutex_destroy(&games[i].mutex_game);
		pthread_cond_destroy(&games[i].cond_player_turn);
		pthread_cond_destroy(&games[i].hasta_que_llegue_el_segundo);
	}
}

/* ===== SERVER MANAGEMENT FUNCTIONS ===== */

/**
 * Process incoming SOAP requests in separate thread
 */
void *processRequest(void *soap)
{
	struct soap *soap_ = (struct soap *)soap;
	soap_serve(soap_);
	soap_destroy(soap_);
	soap_end(soap_);
	soap_done(soap_);
	free(soap_);
	return NULL;
}

/**
 * Main server function
 */
int main(int argc, char **argv)
{

	struct soap soap;
	struct soap *tsoap;
	pthread_t tid;
	int port;
	SOAP_SOCKET m, s;

	// Check arguments
	if (argc != 2)
	{
		printf("Usage: %s port\n", argv[0]);
		exit(0);
	}

	soap_init(&soap);

	// Configure timeouts
	soap.send_timeout = 60;		// 60 seconds
	soap.recv_timeout = 60;		// 60 seconds
	soap.accept_timeout = 3600; // server stops after 1 hour of inactivity
	soap.max_keep_alive = 100;	// max keep-alive sequence

	// Get listening port
	port = atoi(argv[1]);

	// Bind
	m = soap_bind(&soap, NULL, port, 100);

	if (!soap_valid_socket(m))
	{
		exit(1);
	}

	printf("Server is ON!\n");
	initServerStructures(&soap);

	while (TRUE)
	{

		// Accept a new connection
		s = soap_accept(&soap);

		// Socket is not valid :(
		if (!soap_valid_socket(s))
		{

			if (soap.errnum)
			{
				soap_print_fault(&soap, stderr);
				exit(1);
			}

			fprintf(stderr, "Time out!\n");
			break;
		}

		// Copy the SOAP environment
		tsoap = soap_copy(&soap);

		if (!tsoap)
		{
			printf("SOAP copy error!\n");
			break;
		}

		// Create a new thread to process the request
		pthread_create(&tid, NULL, (void *(*)(void *))processRequest, (void *)tsoap);
	}

	// Detach SOAP environment
	soap_done(&soap);

	cleanupServer();

	return 0;
}
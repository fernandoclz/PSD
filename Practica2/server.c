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
	game->player1Bet = 0;
	game->player2Bet = 0;
	game->player1Stack = INITIAL_STACK;
	game->player2Stack = INITIAL_STACK;

	// Game status variables
	game->endOfGame = FALSE;
	game->status = gameEmpty;
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
		games[i].currentPlayer = player1;
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

void copyGameStatusStructure(blackJackns__tBlock *status, char *message, blackJackns__tDeck *newDeck, int newCode)
{

	// Copy the message
	memset((status->msgStruct).msg, 0, STRING_LENGTH);
	strcpy((status->msgStruct).msg, message);
	(status->msgStruct).__size = strlen((status->msgStruct).msg);

	// Copy the deck, only if it is not NULL
	if (newDeck->__size > 0)
		memcpy((status->deck).cards, newDeck->cards, DECK_SIZE * sizeof(unsigned int));
	else
		(status->deck).cards = NULL;

	(status->deck).__size = newDeck->__size;

	// Set the new code
	status->code = newCode;
}

int blackJackns__getStatus(struct soap *soap,
						   blackJackns__tMessage playerName,
						   int gameId,
						   blackJackns__tBlock *result)
{
	allocClearBlock(soap, result);
	playerName.msg[playerName.__size] = 0;

	if (gameId < 0 || gameId >= MAX_GAMES)
	{
		copyGameStatusStructure(result, "Invalid game ID", NULL, ERROR_SERVER_FULL);
		return SOAP_OK;
	}

	tGame *game = &games[gameId];
	pthread_mutex_lock(&game->mutex_status);

	// Verificar si el jugador pertenece a la partida
	int isPlayer1 = strcmp(playerName.msg, game->player1Name) == 0;
	int isPlayer2 = strcmp(playerName.msg, game->player2Name) == 0;
	pthread_mutex_unlock(&game->mutex_status);
	if (!isPlayer1 && !isPlayer2)
	{
		copyGameStatusStructure(result, "Player not found", NULL, ERROR_PLAYER_NOT_FOUND);
		return SOAP_OK;
	}
	pthread_mutex_lock(&game->mutex_game);

	// Esperar si no es su turno
	while ((isPlayer1 && game->currentPlayer != player1) ||
		   (isPlayer2 && game->currentPlayer != player2))
	{
		pthread_cond_wait(&game->cond_player_turn, &game->mutex_game);
	}

	// Si el juego terminó
	if (game->endOfGame)
	{
		copyGameStatusStructure(result, "Game over", NULL, GAME_LOSE);
	}
	else
	{
		copyGameStatusStructure(result, "Your turn!",
								isPlayer1 ? &game->player1Deck : &game->player2Deck,
								TURN_PLAY);
	}

	pthread_mutex_unlock(&game->mutex_game);
	return SOAP_OK;
}

int blackJackns__register(struct soap *soap, blackJackns__tMessage playerName, int *result)
{

	int gameIndex = -1;
	int i = 0, found = 0;

	// Set \0 at the end of the string
	playerName.msg[playerName.__size] = 0;

	if (DEBUG_SERVER)
		printf("[Register] Registering new player -> [%s]\n", playerName.msg);

	while (i < MAX_GAMES && !found)
	{
		pthread_mutex_lock(&games[i].mutex_status);
		tGameState aux_status = games[i].status;
		if (aux_status == gameEmpty)
		{
			strncpy(games[i].player1Name, playerName.msg, STRING_LENGTH - 1);
			games[i].player1Name[STRING_LENGTH - 1] = 0;
			games[i].status = gameWaitingPlayer;
			gameIndex = i;
			found = 1;
			pthread_cond_wait(&games[i].hasta_que_llegue_el_segundo, &games[i].mutex_status);
			pthread_mutex_unlock(&games[i].mutex_status);
			break;
		}
		else if (aux_status == gameWaitingPlayer)
		{
			if (strncmp(playerName.msg, games[i].player1Name, STRING_LENGTH) == 0)
			{
				pthread_mutex_unlock(&games[i].mutex_status);
				*result = ERROR_NAME_REPEATED;
				return SOAP_OK;
			}
			strncpy(games[i].player2Name, playerName.msg, STRING_LENGTH - 1);
			games[i].player2Name[STRING_LENGTH - 1] = 0;
			games[i].status = gameReady;
			gameIndex = i;
			found = 1;

			pthread_cond_broadcast(&games[i].hasta_que_llegue_el_segundo);
			pthread_mutex_unlock(&games[i].mutex_status);
			break;
		}
		else
		{
			pthread_mutex_unlock(&games[i].mutex_status);
		}
		i++;
	}
	if (gameIndex == -1)
	{
		*result = ERROR_SERVER_FULL;
	}
	else
	{
		*result = gameIndex;
	}
	if (DEBUG_SERVER)
		printf("[Register] %s in game %d", playerName.msg, gameIndex);

	return SOAP_OK;
}

int blackJackns__playerMove(struct soap *soap,
							blackJackns__tMessage playerName,
							int gameId,
							unsigned int move,
							blackJackns__tBlock *result)
{
	allocClearBlock(soap, result);
	playerName.msg[playerName.__size] = '\0';

	// Validaciones básicas
	if (gameId < 0 || gameId >= MAX_GAMES)
	{
		copyGameStatusStructure(result, "Invalid game ID", NULL, ERROR_SERVER_FULL);
		return SOAP_OK;
	}

	tGame *game = &games[gameId];
	pthread_mutex_lock(&game->mutex_status);

	int isPlayer1 = strcmp(playerName.msg, game->player1Name) == 0;
	int isPlayer2 = strcmp(playerName.msg, game->player2Name) == 0;
	if (!isPlayer1 && !isPlayer2)
	{
		copyGameStatusStructure(result, "Player not found", NULL, ERROR_PLAYER_NOT_FOUND);
		return SOAP_OK;
	}
	pthread_mutex_lock(&game->mutex_game);

	// Si ya terminó la partida
	if (game->endOfGame)
	{
		pthread_mutex_unlock(&game->mutex_game);
		copyGameStatusStructure(result, "Game already finished", NULL, GAME_LOSE);
		return SOAP_OK;
	}

	// Comprobar turno actual
	if ((isPlayer1 && game->currentPlayer != player1) ||
		(isPlayer2 && game->currentPlayer != player2))
	{
		pthread_mutex_unlock(&game->mutex_game);
		copyGameStatusStructure(result, "Not your turn", NULL, TURN_WAIT);
		return SOAP_OK;
	}

	// Seleccionar mazos del jugador y rival
	blackJackns__tDeck *myDeck = isPlayer1 ? &game->player1Deck : &game->player2Deck;
	blackJackns__tDeck *rivalDeck = isPlayer1 ? &game->player2Deck : &game->player1Deck;

	// Acción del jugador
	if (move == PLAYER_HIT_CARD)
	{
		// Robar carta
		unsigned int card = getRandomCard(&game->gameDeck);
		myDeck->cards[myDeck->__size++] = card;

		unsigned int myPoints = calculatePoints(myDeck);

		char msg[STRING_LENGTH];
		snprintf(msg, STRING_LENGTH, "You hit a card. Now you have %u points.", myPoints);

		// Si el jugador se pasa -> pierde
		if (myPoints > GOAL_GAME)
		{
			game->endOfGame = TRUE;
			copyGameStatusStructure(result, "You busted! You lose.", myDeck, GAME_LOSE);
			pthread_cond_signal(&game->cond_player_turn); // Despertar rival
			pthread_mutex_unlock(&game->mutex_game);
			return SOAP_OK;
		}
		else if (myPoints == GOAL_GAME)
		{
			game->endOfGame = TRUE;
			copyGameStatusStructure(result, "You reached 21! You win!", myDeck, GAME_WIN);
			pthread_cond_signal(&game->cond_player_turn);
			pthread_mutex_unlock(&game->mutex_game);
			return SOAP_OK;
		}
		else
		{
			// Continúa su turno
			copyGameStatusStructure(result, msg, myDeck, TURN_PLAY);
			pthread_mutex_unlock(&game->mutex_game);
			return SOAP_OK;
		}
	}
	else if (move == PLAYER_STAND)
	{
		// Cambiar turno al rival
		game->currentPlayer = calculateNextPlayer(game->currentPlayer);

		char msg[STRING_LENGTH];
		snprintf(msg, STRING_LENGTH, "You stand. Waiting for your opponent...");

		copyGameStatusStructure(result, msg, myDeck, TURN_WAIT);

		pthread_cond_signal(&game->cond_player_turn); // Despertar rival
		pthread_mutex_unlock(&game->mutex_game);
		return SOAP_OK;
	}
	else
	{
		pthread_mutex_unlock(&game->mutex_game);
		copyGameStatusStructure(result, "Invalid move", NULL, TURN_PLAY);
		return SOAP_OK;
	}
}

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

	return 0;
}
#include "client.h"

unsigned int readBet()
{

	int isValid, bet = 0;
	xsd__string enteredMove;

	// While player does not enter a correct bet...
	do
	{

		// Init...
		enteredMove = (xsd__string)malloc(STRING_LENGTH);
		bzero(enteredMove, STRING_LENGTH);
		isValid = TRUE;

		printf("Enter a value:");
		fgets(enteredMove, STRING_LENGTH - 1, stdin);
		enteredMove[strlen(enteredMove) - 1] = 0;

		// Check if each character is a digit
		for (int i = 0; i < strlen(enteredMove) && isValid; i++)
			if (!isdigit(enteredMove[i]))
				isValid = FALSE;

		// Entered move is not a number
		if (!isValid)
			printf("Entered value is not correct. It must be a number greater than 0\n");
		else
			bet = atoi(enteredMove);

	} while (!isValid);

	printf("\n");
	free(enteredMove);

	return ((unsigned int)bet);
}

unsigned int readOption()
{

	unsigned int bet;

	do
	{
		printf("What is your move? Press %d to hit a card and %d to stand\n", PLAYER_HIT_CARD, PLAYER_STAND);
		bet = readBet();
		if ((bet != PLAYER_HIT_CARD) && (bet != PLAYER_STAND))
			printf("Wrong option!\n");
	} while ((bet != PLAYER_HIT_CARD) && (bet != PLAYER_STAND));

	return bet;
}

int main(int argc, char **argv)
{

	struct soap soap;				  /** Soap struct */
	char *serverURL;				  /** Server URL */
	blackJackns__tMessage playerName; /** Player name */
	blackJackns__tBlock gameStatus;	  /** Game status */
	unsigned int playerMove;		  /** Player's move */
	int resCode, gameId = -3;		  /** Result and gameId */
	int endOfGame = 0;

	// Check arguments
	if (argc != 2)
	{
		printf("Usage: %s http://server:port\n", argv[0]);
		exit(0);
	}
	// Init gSOAP environment
	soap_init(&soap);

	// Obtain server address
	serverURL = argv[1];

	// Allocate memory
	allocClearMessage(&soap, &(playerName));
	allocClearBlock(&soap, &gameStatus);

	printf("Introduce your username: ");
	if (fgets(playerName.msg, STRING_LENGTH, stdin) == NULL)
	{
		fprintf(stderr, "Error leyendo username\n");
		return 1;
	}
	size_t ln = strlen(playerName.msg);
	if (ln > 0 && playerName.msg[ln - 1] == '\n')
		playerName.msg[ln - 1] = '\0';
	playerName.__size = (int)strlen(playerName.msg);

	do
	{
		resCode = soap_call_blackJackns__register(&soap, serverURL, "", playerName, &gameId);
		if (resCode != SOAP_OK)
		{
			soap_print_fault(&soap, stderr); // Muestra el error SOAP detallado
			fprintf(stderr, "Error llamando al servidor (resCode=%d)\n", resCode);
			break; // o return 1;
		}
		if (gameId == ERROR_SERVER_FULL)
		{
			printf("Sorry, not available\n");
			endOfGame = 1;
		}
		else if (gameId == ERROR_NAME_REPEATED)
		{
			printf("Name repeated, choose another name\n");
			endOfGame = 1;
		}
	} while (gameId < 0 && !endOfGame);

	if (gameId >= 0)
	{
		printf("Usuario registrado correctamente\n");
	}

	while (endOfGame == 0)
	{
		int res = soap_call_blackJackns__getStatus(&soap, serverURL, "", playerName, gameId, &gameStatus);
		if (res != SOAP_OK)
		{
			soap_print_fault(&soap, stderr);
			break;
		}
		printStatus(&gameStatus, FALSE);

		if (DEBUG_CLIENT)
			showCodeText(gameStatus.code);

		switch (gameStatus.code)
		{
		case TURN_PLAY:
			playerMove = readOption();
			int res = soap_call_blackJackns__playerMove(&soap, serverURL, "", playerName, gameId, playerMove, &gameStatus);
			if (res != SOAP_OK)
				soap_print_fault(&soap, stderr);
			break;
		case TURN_WAIT:
			printStatus(&gameStatus, FALSE);
			break;
		case GAME_WIN:
			printf("You won the game!\n");
			printStatus(&gameStatus, FALSE);
			printf("\n");

			endOfGame = 1;
			break;
		case GAME_LOSE:
			printf("You lost the game!\n");
			printStatus(&gameStatus, FALSE);

			printf("\n");

			endOfGame = 1;
			break;
		case ERROR_PLAYER_NOT_FOUND:
			printf("Error: player not found.\n");
			endOfGame = 1;
			break;
		default:
			printf("Unkwon code: %d\n", gameStatus.code);
			printStatus(&gameStatus, FALSE);
			endOfGame = 1;
			break;
		}
	}

	soap_end(&soap);
	soap_done(&soap);
	return 0;
}

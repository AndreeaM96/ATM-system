#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 20
#define BUFLEN 256

typedef struct user_data {		//Structura folosita pentru a stoca datele aferente unul client (utilizator)
	char first_name[12];		//Prenume
	char last_name[12];			//Nume
	unsigned int card_number;	//Nr card
	unsigned int pin;			//Pinul
	char secret_pass[16];		//Parola secreta
	double balance;				//Soldul
	int card_block;				//O sa aiba valoarea 0 sau 1. Folosit pentru retinerea starii cardului (daca este blocat sau nu)
	int active_session;			//O sa aiba valoarea 0 sau 1. Folosit pentru retinerea logarii (0 daca nu exista sesiune activa
								//pentru utilizatorul respectiv, 1 daca exista).
	int wrong_count;			//Retine numarul de introduceri gresite a pinului. Daca are valoarea 3, cardul se blocheaza.
} user;

void process_command(char buffer[], user users[], int n){	//Functia care proceseaza comenzile date in buffer (cu exceptia
															//comenzii quit)
	char aux[16];
	int exists = 0;				//In aceasta variabila o sa retin daca exista numarul de card dat ca argument
	int i;
	memset(aux, 0, 16);
	memcpy(aux, buffer, 4);			//Retin in aux primele 4 litere ale comenzii (destul pentru a determina ce comanda am).
	if(strcmp(aux, "logi") == 0){	//In cazul in care am login
		char aux2[16];
		unsigned int card_no;
		unsigned int read_pin;
		memset(aux2, 0, 16);
		sscanf(buffer,"%s %u %u\n",aux2, &card_no, &read_pin);	//Salvez nr card si pinul date ca argument.
		for(i = 0 ; i < n ; i++){				//Iau pe rand toti userii pentru a gasii userul cautat
			if(exists == 1){					//Daca deja am gasit userul, ies din for
				break;
			}
			if(users[i].card_number == card_no){		//Daca am gasit un user care se potriveste numarului de card salvat
				exists = 1;								//Marchez in variabila ca am gasit userul
				if(users[i].active_session == 1){		//Daca deja am o sesiune activa pentru userul respectiv
					memset(buffer, 0, BUFLEN);
					sprintf(buffer,"ATM> -2 : Sesiune deja deschisa\n");	//Pun mesajul corespunzator in buffer
					break;
				}
				if(users[i].pin == read_pin && users[i].card_block == 0){	//Daca pinul se potriveste si cardul nu este blocat
					memset(buffer, 0, BUFLEN);
					sprintf(buffer,"ATM> Welcome %s %s\n", users[i].last_name, users[i].first_name);	//Pun mesajul corespunzator
																										//in buffer
					users[i].wrong_count = 0;			//Setez la 0 variabila care retine greselile
					users[i].active_session = 1;		//Marchez faptul ca acum am o sesiune activa pentru acest user		
				}
				else {								//Daca pinul nu se potriveste
					memset(buffer, 0, BUFLEN);
					sprintf(buffer,"ATM> -3 : Pin gresit\n");	//Pun mesajul corespunzator in buffer
					users[i].wrong_count++;						//Incrementez variabila care retine greselile
				}
				if(users[i].wrong_count >= 3){					//Daca variabila care retine greselile are valoarea 3
					users[i].card_block = 1;					//Marchez cardul ca blocat
				}
				if(users[i].card_block == 1){					//Daca cardul este blocat
					memset(buffer, 0, BUFLEN);
					sprintf(buffer,"ATM> -5 : Card blocat\n");	//Pun mesajul corespunzator in buffer
				}
			}
		}
		if(exists == 0){		//Daca am terminat bucla si exists == 0, atunci inseamna ca nu exista un user cu numarul de card dat
			memset(buffer, 0, BUFLEN);
			sprintf(buffer,"ATM> -4 : Numar card inexistent\n");	//Pun mesajul corespunzator in buffer
		}
	}
	if(strcmp(aux, "logo") == 0){					//In cazul in care am logout
		unsigned int aux_cardno;
		int i;
		sscanf(buffer,"logout\n %u", &aux_cardno);		//Extrag numarul de card din buffer
		for(i = 0 ; i < n ; i++){						//Trec prin toti userii
			if(users[i].card_number == aux_cardno){		//Daca gasesc numarul de card
				users[i].active_session = 0;			//Marche faptul ca nu mai am sesiune activa pentru acest user
				users[i].wrong_count = 0;				//Setez la 0 variabila care retine greselile
			}
		}
		memset(buffer, 0, BUFLEN);
		sprintf(buffer,"ATM> Deconectare de la bancomat\n");	//Pun mesajul corespunzator in buffer
	}
	if(strcmp(aux, "list") == 0){					//In cazul in care am listsold
		int i;
		unsigned int aux_cardno;
		sscanf(buffer,"listsold\n %u", &aux_cardno);				//Extrang numarul de card din buffer
		for(i = 0 ; i < n ; i++){									//Iterez prin useri
			if(users[i].card_number == aux_cardno){					//Daca gasesc numarul de card
				memset(buffer, 0, BUFLEN);
				sprintf(buffer,"ATM> %.2lf\n", users[i].balance);	//Pun mesajul corespunzator in buffer
			}
		}
	}
	if(strcmp(aux, "getm") == 0){					//In cazul in care am getmoney
		unsigned int aux_cardno;
		int amount, i;
		sscanf(buffer,"getmoney %d\n %u", &amount, &aux_cardno);	//Extrag din buffer numarul de card si suma de bani
		for(i = 0 ; i < n ; i++){									//Trec prin toti userii
			if(users[i].card_number == aux_cardno){					//Daca am gasit userul cu numarul de card respectiv
				if(amount % 10 != 0){								//Daca suma data nu este multiplu de 10
					memset(buffer, 0, BUFLEN);
					sprintf(buffer, "ATM> -9 : Suma nu este multiplu de 10\n");	//Pun mesajul corespunzator in buffer
				}
				else {												//Daca suma este multiplu de 10
					if(amount > users[i].balance){					//Daca nu am fonduri suficiente
						memset(buffer, 0, BUFLEN);
						sprintf(buffer, "ATM> -8 : Fonduri insuficiente\n");			//Pun mesajul corespunzator in buffer
					}
					else {											//Daca am fonduri suficiente
						users[i].balance -= amount;					//Extrag banii
						memset(buffer, 0, BUFLEN);
						sprintf(buffer, "ATM> Suma %d retrasa cu succes\n", amount);	//Pun mesajul corespunzator in buffer
					}
				}
			}
		}
	}
	if(strcmp(aux, "putm") == 0){					//In cazul in care am putmoney
		unsigned int aux_cardno;
		double amount;
		int i;
		sscanf(buffer,"putmoney %lf\n %u", &amount, &aux_cardno);	//Extrag numarul de card si suma de bani de depus
		for(i = 0 ; i < n ; i++){									//Iterez prin useri
			if(users[i].card_number == aux_cardno){					//Daca am gasit userul cautat
				users[i].balance += amount;							//Depun banii
				memset(buffer, 0, BUFLEN);
				sprintf(buffer, "ATM> Suma depusa cu succes\n");	//Pun mesajul corespunzator in buffer
			}
		}
	}
}

void error(char* msg){
	perror(msg);
	exit(1);
}

int max(int a, int b){						//Functie pentru aflarea maximului (pentru aflarea fdmax)
	if(a > b)
		return a;
	else
		return b;
}

int main(int argc, char *argv[]){
	int n, i, j, m;
	int on = 1;
	int port = atoi(argv[1]);				//Portul este dat ca prim argument
 	FILE* f = fopen(argv[2], "r");			//Fisierul cu datele clientilor este al doilea argument

	int tcpsock, newsockfd, udpsock, clilen;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
	fd_set read_fds;
    fd_set tmp;
    int fdmax;	
	FD_ZERO(&read_fds);						//Initializez cele doua fd_seturi
    FD_ZERO(&tmp);

	fscanf(f, "%d\n", &n);
	user users[n];							//Vectorul de useri
	
	for(i = 0 ; i < n ; i++){				//Citesc datele din fisier
		fscanf(f, "%s %s %u", users[i].last_name, users[i].first_name, &users[i].card_number);
		fscanf(f, " %u %s %lf\n", &users[i].pin, users[i].secret_pass, &users[i].balance);
		users[i].card_block = 0;			//Initializez celelalte variabile din structura cu useri
		users[i].active_session = 0;
		users[i].wrong_count = 0;
	}


	tcpsock = socket(AF_INET, SOCK_STREAM, 0);				//Creez socketul TCP
    if (tcpsock < 0) 
		error("ERROR opening TCP socket");
    
	memset((char *) &serv_addr, 0, sizeof(serv_addr));		//Creez structura care o sa retina adresa serverului
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
	setsockopt(tcpsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
     
    if(bind(tcpsock, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)	//Leg socketul TCP la adresa creeata anterior
		error("ERROR on binding TCP");

	udpsock = socket(AF_INET, SOCK_DGRAM, 0);				//Creez socketul UDP
	if (udpsock < 0) 
		error("ERROR opening UDP socket");

	if(bind(udpsock, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)	//Leg socketul UDP la adresa creata anterior
		error("ERROR on binding UDP");
     
    listen(tcpsock, MAX_CLIENTS);						//Ascult pe socketul TCP pentru conexiuni noi
	FD_SET(0, &read_fds);								//Adaug in fd_set socketurile care o sa fie multiplexate
	FD_SET(tcpsock, &read_fds);
	FD_SET(udpsock, &read_fds);
    fdmax = max(udpsock, tcpsock);

	while(1){
		tmp = read_fds; 								//Folosesc tmp in loc de read_fds pentru a nu afecta multimea read_fds
														//in urma apelului select
		if (select(fdmax + 1, &tmp, NULL, NULL, NULL) == -1) 
			error("ERROR in select");

		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp)) {
				if(i == udpsock){							//Daca mi-a venit ceva pe socketul UDP
					memset(buffer, 0, BUFLEN);
					clilen = sizeof(cli_addr);
					m = recvfrom(i, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, &clilen);	//Citesc de pe socketul UDP
					if (m <= 0) {
							error("ERROR in recvfrom");
					}
					else {	
						char aux[16];
						memset(aux, 0, 16);
						memcpy(aux, buffer, 4);			//Salvez in aux primele 4 litere din buffer, pentru a imi da seama daca
														//chiar am primit comanda unlock
						if(strcmp(aux, "unlo") == 0){
							unsigned int aux_cardno;
							int exists = 0;
							sscanf(buffer,"unlock\n %u", &aux_cardno);	//Imi salvez numarul de card pe care se va incerca deblocarea
							for(j = 0 ; j < n ; j++){
								if(exists == 1)								//Daca deja am gasit clientul respectiv, ies din bucla
									break;
								if(users[j].card_number == aux_cardno){		//Daca am gasit clientul cu nr de card corespunzator
									char aux_spass[16];
									exists = 1;								//Marchez ca l-am gasit
									memset(aux_spass, 0, 16);
									memset(buffer, 0, BUFLEN);
									if(users[j].card_block == 0){			//Daca cardul nu este blocat, pun mesaj de eroare in buffer
										sprintf(buffer, "UNLOCK> -6 : Operatie esuata\n");
									}
									else {									//Daca cardul este blocat
										sprintf(buffer, "UNLOCK> Trimite parola secreta\n");			//Cer parola secreta
										sendto(i, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, clilen);	//Trimit cererea
										memset(buffer, 0 , BUFLEN);
										recvfrom(i, buffer, sizeof(buffer), 0, NULL, NULL);		//Primesc raspunsul
										sscanf(buffer, "%s\n", aux_spass);						//Il salvez in variabila
										if(strcmp(aux_spass, users[j].secret_pass) == 0){	//Daca parola se potriveste
											users[j].wrong_count = 0;						//Resetez numarul de greseli
											users[j].card_block = 0;						//Deblochez cardul
											memset(buffer, 0, BUFLEN);
											sprintf(buffer,"UNLOCK> Client deblocat\n");	//Pun mesajul de confirmare in buffer
										}
										else {
											memset(buffer, 0, BUFLEN);
											sprintf(buffer,"UNLOCK> -7 : Deblocare esuata\n");	//Daca parola nu se potriveste, pun
																								//mesaj de eroare in buffer
										}
									}
								}
							}
							if(exists == 0){							//Daca nu s-a gasit user cu numarul de card dat
								memset(buffer, 0, BUFLEN);
								sprintf(buffer,"UNLOCK> -4 : Numar card inexistent\n");	//Pun mesajul de eroare in buffer
							}
							m = sendto(i, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, clilen);	//Trimit mesajul final
							if (m <= 0)
								error("ERROR in sendto");
						}
					}
					continue;							//Continue pentru a trece la urmatoarea iteratie din for
				}
				if(i == 0){								//Daca am primit ceva de la tastatura (pe partea serverului)
					fgets(buffer, BUFLEN-1, stdin);		//Citesc de la tastatura
					char aux[16];
					memset(aux, 0, 16);
					memcpy(aux, buffer, 4);				//Pun in aux primele 4 litere
					if(strcmp(aux, "quit") == 0){		//Daca cele 4 litere sunt "quit"
						memset(buffer, 0, BUFLEN);
						sprintf(buffer, "Serverul se inchide...Vei fi deconectat.\n");	//Pun in buffer mesajul de inchidere care
																		//trebuie trimis tuturor clientilor
						for(j = 1; j <= fdmax; j++){					//Iau pe rand toti file descriptorii
							if(FD_ISSET(j, &read_fds) && j != udpsock && j != tcpsock) {
								send(j, buffer, sizeof(buffer), 0);		//Trimit mesajul de inchidere
								close(j);							//Inchid socketul deschis pentru comunicarea cu clientul respectiv
								FD_CLR(j, &read_fds);				//Scot fd-ul din lista de fd-uri
							}
						}
						exit(1);							//La final inchid serverul
					}
					continue;							//Continue pentru a trece la urmatoarea iteratie din for
				}
				if (i == tcpsock) {						//A venit ceva pe socketul pe care am facut listen
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(tcpsock, (struct sockaddr *)&cli_addr, &clilen)) == -1) {	//Fac accept pe noua conexiune
						error("ERROR in accept");
					} 
					else {
						FD_SET(newsockfd, &read_fds);	//Adaug noul socket in fd_setul meu
						if (newsockfd > fdmax) {		//Schimb fdmax daca e cazul
							fdmax = newsockfd;
						}
					}
				}
				else {				//Altfel, inseamna ca am primit ceva de la unul din socketii pe care and dat accept
					memset(buffer, 0, BUFLEN);
					if ((m = recv(i, buffer, sizeof(buffer), 0)) <= 0) {		//Fac recv pe socketul respectiv
						if (m == 0) {				//Daca nu primesc nimic, atunci inseamna ca s-a inchis fortat conexiunea
							printf("socket %d hung up\n", i);
						} else {
							error("ERROR in recv");
						}
						close(i); 					//Inchid socketul care s-a deconectat
						FD_CLR(i, &read_fds); 		//Scot din read_fds socketul inchis 
					} 
					
					else { 							//Altfel, am primit ceva pe socket
						char aux[16];
						memset(aux, 0, 16);
						memcpy(aux, buffer, 4);
						if(strcmp(aux, "quit") == 0){		//Verific daca am primit mesaj de quit de la client
							int j;
							unsigned int aux_cardno;
							sscanf(buffer,"quit\n %u", &aux_cardno);	//Daca da, atunci salvez numarul de card primit pentru a
																		//marca sesiunea cu acel client ca inchisa
							for(j = 0 ; j < n ; j++){					//Caut clientul cu acel nr de card
								if(users[j].card_number == aux_cardno){
									users[j].active_session = 0;		//Daca l-am gasit, atunci setez faptul ca nu mai are
																		//o sesiune deja deschisa
								}
							}
							close(i);						//Inchid socketul respectiv
							FD_CLR(i, &read_fds);			//Il scot din lista de fduri
						}
						else {										//Altfel, daca nu am primit comanda de quit
							process_command(buffer, users, n);		//O trimit functiei de procesare, care imi va modifica
																	//bufferul in mod corespunzator
							send(i, buffer, sizeof(buffer), 0);		//Trimit rezultatul
						}
					}
				}
			}
		}
	}
	return 0;
}

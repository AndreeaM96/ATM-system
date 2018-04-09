#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BUFLEN 256

void error(char* msg){
	perror(msg);
	exit(1);
}

int main(int argc, char *argv[]){
	int tcpsock, udpsock, n, fdmax, i, addr_len;
	int is_logged_in = 0;
	unsigned int current_cardno, logged_in_card;
	unsigned int current_pin;
	char procid[10];
	sprintf(procid, "%d", getpid());	//Obtin ID-ul procesului
	char filename[25] = "client-";		//Incep sa construiesc numele fisierului
	strcat(filename, procid);			//Atasez numarul procesului la nume
	strcat(filename, ".log");			//Atasez extensia
	FILE* f = fopen(filename, "a");		//Creez si deschid fisierul

    struct sockaddr_in serv_addr;
    struct hostent *server;

	fd_set set, tmp;					//Declar si initializez listele de fd-uri
	FD_ZERO(&set);
	FD_ZERO(&tmp);

    char buffer[BUFLEN];
    
	tcpsock = socket(AF_INET, SOCK_STREAM, 0);		//Creez socketul TCP pe care voi comunica cu serverul
	udpsock = socket(AF_INET, SOCK_DGRAM, 0);		//Creez socketul UDP pe care voi comunica cu serverul
    if (tcpsock < 0) 
        error("ERROR opening TCP socket");
	if (udpsock < 0) 
        error("ERROR opening UDP socket");
    
    serv_addr.sin_family = AF_INET;					//Configurez structura in care retin adresa serverului
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);
    
    if (connect(tcpsock,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0) 	//Ma conectez la server pe portul TCP creat
        error("ERROR connecting TCP");

	FD_SET(0, &set);								//Adaug in fd_set toate socketurile pe care le voi multiplexa
	FD_SET(tcpsock, &set);
	FD_SET(udpsock, &set); 
	fdmax = udpsock; 
    
    while(1){
		tmp = set;							//Salvez in tmp pe read_set, pentru a nu strica read_set cand apelez select
		if(select(fdmax + 1, &tmp, NULL, NULL, NULL) == -1)
			error("ERROR in select");

		for(i = 0 ; i <= fdmax ; i++){
			if (FD_ISSET(i, &tmp)) {
				if(i == tcpsock){					//Daca am primit ceva pe TCP
					memset(buffer, 0 , BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {	//Fac recv si verific pentru erori
						if (n == 0) {				//Daca nu am primit nimic, conexiunea a fost inchisa fortat
							exit(1);
						} else {
							error("ERROR in recv");
						}
						FD_CLR(i, &set); 			//Scot din fd_set socketul inchis 
					} 
					
					else {					//Altfel, am primit un mesaj pe TCP
						if(strcmp(buffer,"Serverul se inchide...Vei fi deconectat.\n") == 0){	//Daca am primit mesaj ca
																								//serverul se inchide
							printf("%s", buffer);					//Il afisez userului
							fprintf(f, "%s", buffer);				//Il printez in fisierul log
							fprintf(f, "Conexiunea s-a incheiat\n");	//Afisez userului fatul ca conexiunea s-a inchis
							printf("Conexiunea s-a incheiat\n");	//Printez si in fisierul log
							fclose(f);						//Inchid fisierul
							FD_CLR(tcpsock, &set);			//Scot toti socketii din fd_set
							FD_CLR(udpsock, &set);
							FD_CLR(0, &set);
							exit(1);						//Inchid cientul
						}
					}
				}
				else {						//Altfel, am primit o comanda de la user
					fgets(buffer, BUFLEN-1, stdin);		//Citesc de la tastatura
					char aux[16];
					memset(aux, 0, 16);
					memcpy(aux, buffer, 4);				//Salvez primele 4 litere pentru a imi da seama ce comanda am primit
					if(strcmp(aux, "logi") == 0){		//Daca am primit comanda login
						fprintf(f, "%s", buffer);		//O scriu in fisier
						if(is_logged_in == 1){			//Daca am deja un user logat pe clientul acesta
							printf("-2 : Sesiune deja deschisa\n");			//Printez eroare
							fprintf(f, "-2 : Sesiune deja deschisa\n");		//Pun eroarea in fisier
						}
						else {							//Altfel, daca nu e nimeni logat pe clientul asta
							sscanf(buffer,"login %u %u\n", &current_cardno, &current_pin);	//Iau nr de card si pinul pentru a
																				//retine cine a facut ultima incercare de login
							send(tcpsock, buffer, strlen(buffer), 0);			//Trimit serverului comanda
							recv(tcpsock, buffer, sizeof(buffer), 0);			//Iau rezultatul
							char aux2[16];
							memset(aux2, 0, 16);
							memcpy(aux2, &buffer[6], 1);					//Copiez ce e pe pozitia 6
														//Explicatie: pe a sasea pozitie se vor gasii ori cifere
														//3/4/5 (in caz ca am eroare) ori o litera, in caz de logare cu succes
							strcat(aux2,"\0");
							int errcode = atoi(aux2);
							
							if(errcode != 3 && errcode != 4 && errcode != 5){	//In caz ca nu am eroare
								is_logged_in = 1;					//Setez faptul ca in clientul acesta am un user logat
								logged_in_card = current_cardno;	//In variabila asta salvez ultimul nr de card care s-a
																	//autentificat cu succes
							}
							
							printf("%s", buffer);			//Afisez mesajul final (eroarea/autentificarea cu succes)
							fprintf(f, "%s", buffer);		//Si il scriu si in fisier
						}
					}

					if(strcmp(aux, "logo") == 0){			//Daca am primit comanda logout
						fprintf(f, "%s", buffer);			//Pun comanda in fisier
						
						if(is_logged_in == 0){				//Daca nu am user logat in aces client
							printf("-1 : Clientul nu este autentificat\n");			//Printez eroarea
							fprintf(f, "-1 : Clientul nu este autentificat\n");		//Si o pun si in fisier
						}
						else {						//Altfel, daca am pe cineva logat aici
							is_logged_in = 0;		//Setez faptul ca nimeni nu mai e logat in clientul curent
							char aux2[16];
							memset(aux2, 0, 16);
							sprintf(aux2, "%u", current_cardno);	//Adaug la mesajul de logout nr cardului care s-a logat
														//pentru a il transmite serverului si a il informa ca acel client nu mai
														//are o sesiune deschisa
							strcat(buffer, " ");
							strcat(buffer,aux2);
							send(tcpsock, buffer, strlen(buffer), 0);		//Trimit mesajul
							recv(tcpsock, buffer, sizeof(buffer), 0);		//Primesc raspunsul de la server
							printf("%s", buffer);							//Il printez userului
							fprintf(f, "%s", buffer);						//Il un si in fisier
						}
					}
					if(strcmp(aux, "list") == 0){				//Daca am primit comanda listsold
						fprintf(f, "%s", buffer);				//Pun comanda in fisier
						if(is_logged_in == 0){					//Daca nu am vreun user logat in client
							printf("-1 : Clientul nu este autentificat\n");			//Printez eroarea
							fprintf(f, "-1 : Clientul nu este autentificat\n");		//Si o pun si in fisier
						}
						else {								//Altfel, daca am user logat
							char aux2[16];
							memset(aux2, 0, 16);
							sprintf(aux2, "%u", current_cardno);	//Adaug numarul de card la mesaj pentru ca serverul sa stie
																	//soldul cui sa il afiseze
							strcat(buffer, " ");
							strcat(buffer,aux2);

							send(tcpsock,buffer,strlen(buffer), 0);			//Trimit mesajul
							recv(tcpsock, buffer, sizeof(buffer), 0);		//Primesc raspunsul
							printf("%s", buffer);							//Printez raspunsul
							fprintf(f, "%s", buffer);						//Si il pun si in fisier
						}
					}
					if(strcmp(aux, "getm") == 0){				//Daca am primit comanda getmoney
						fprintf(f, "%s", buffer);				//O pun in fisier
						if(is_logged_in == 0){					//Daca nu am user logat in client
							printf("-1 : Clientul nu este autentificat\n");				//Printez eroarea
							fprintf(f, "-1 : Clientul nu este autentificat\n");			//Si o pun si in fisier
						}
						else {									//Altfel, daca am user logat in client
							char aux2[16];
							memset(aux2, 0, 16);
							sprintf(aux2, "%u", current_cardno);	//Adaug numarul de card la mesaj pentru ca serverul sa stie
																	//carui client sa ii scoata banii
							strcat(buffer, " ");
							strcat(buffer, aux2);

							send(tcpsock,buffer,strlen(buffer), 0);			//Trimit mesajul
							recv(tcpsock, buffer, sizeof(buffer), 0);		//Primesc raspuns
							printf("%s", buffer);							//Afisez raspunsul
							fprintf(f, "%s", buffer);						//Il scriu si in fisier
						}
					}
					if(strcmp(aux, "putm") == 0){		//Daca am primit comanda putmoney
						fprintf(f, "%s", buffer);		//Pun comanda in fisier
						if(is_logged_in == 0){			//Daca nu am user logat in client
							printf("-1 : Clientul nu este autentificat\n");			//Printez eroarea
							fprintf(f, "-1 : Clientul nu este autentificat\n");		//Pun eroarea in fisier
						}
						else {							//Altfel, daca am user logat in client
							char aux2[16];
							memset(aux2, 0, 16);
							sprintf(aux2, "%u", current_cardno);	//Adaug numarul de card la mesaj pentru ca serverul sa stie
																	//carui client sa ii depuna banii
							strcat(buffer, " ");
							strcat(buffer, aux2);

							send(tcpsock,buffer,strlen(buffer), 0);		//Trimit mesajul
							recv(tcpsock, buffer, sizeof(buffer), 0);	//Primesc raspuns
							printf("%s", buffer);						//Printez raspunsul
							fprintf(f, "%s", buffer);					//Si il pun in fisier
						}
					}
					if(strcmp(aux, "unlo") == 0){			//Daca am primit comanda unlock
						fprintf(f, "%s", buffer);			//Pun comanda in fisier
						char aux2[16];
						memset(aux2, 0, 16);
						sprintf(aux2, "%u", current_cardno);	//Adaug numarul de card la mesaj pentru ca serverul sa stie
																//carui client sa incerce sa ii deblocheze cardul
						strcat(buffer, " ");
						strcat(buffer, aux2);

						addr_len = sizeof(serv_addr);
						sendto(udpsock, buffer, sizeof(buffer), 0, (struct sockaddr *)&serv_addr, addr_len);	//Trimit mesajul pe
																												//socketul UDP
						memset(buffer, 0 , BUFLEN);
						recvfrom(udpsock, buffer, sizeof(buffer), 0, (struct sockaddr *)&serv_addr, &addr_len);	//Primesc raspunsul
						if(strcmp(buffer, "UNLOCK> Trimite parola secreta\n") == 0){	//Daca am primit solicitare pentru a trimite
																						//parola secreta
							printf("%s", buffer);						//Printez raspunsul
							fprintf(f, "%s", buffer);					//Si il pun si in fisier

							memset(buffer, 0 , BUFLEN);
							fgets(buffer, BUFLEN-1, stdin);				//Citesc de la tastatura ce input a dat userul
							fprintf(f, "%s", buffer);					//Pun in fisier parola data de client

							sendto(udpsock, buffer, sizeof(buffer), 0, (struct sockaddr *)&serv_addr, addr_len);	//Trimit mesajul
							recvfrom(udpsock, buffer, sizeof(buffer), 0, NULL, NULL);		//Primesc raspunsul de la server
							printf("%s", buffer);											//Printez raspunsul
							fprintf(f, "%s", buffer);										//Pun raspunsul in fisier
						}
						else {								//Altfel, inseama ca am primit eroare si nu mai continui
							printf("%s", buffer);			//Printez eroarea
							fprintf(f, "%s", buffer);		//Si o pun in fisier
						}
					}
					if(strcmp(aux, "quit") == 0){		//Daca am primit comanda quit
						fprintf(f, "%s", buffer);		//Scriu comanda in fisier
						char aux2[16];
						memset(aux2, 0, 16);
						sprintf(aux2, "%u", logged_in_card);		//Adaug la mesaj numarul de card al userului logat curent pentru
												//ca serverul sa stie carui client sa ii seteze faptul ca nu mai are o sesiune activa
						strcat(buffer, " ");
						strcat(buffer, aux2);
						send(tcpsock,buffer,strlen(buffer), 0);		//Trimit mesajul catre server pentru a il instiinta ca voi
																	//inchide conexiunea
						printf("Conexiunea s-a incheiat\n");		//Printez mesajul de inchidere
						fprintf(f, "Conexiunea s-a incheiat\n");	//Si il scriu si in fisier
						fclose(f);									//Inchid fisierul
						FD_CLR(tcpsock, &set);						//Scot toti socketii din fd_set
						FD_CLR(udpsock, &set);
						FD_CLR(0, &set);
						exit(1);									//Inchid clientul
					}
				}
			}
		}
    }
	return 0;
}

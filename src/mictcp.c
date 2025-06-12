#include <mictcp.h>
#include <api/mictcp_core.h>
#include <pthread.h>
#define MAX_SOCKET 12
#define MAX_SYNACK_RESEND 10
#define MAX_TIME_WAIT 500
#define TAILLEF 10
int compteur_socket;

pthread_cond_t syn_condition;
pthread_cond_t ack_condition;
pthread_mutex_t wait_mutex;

mic_tcp_sock tab_sock[MAX_SOCKET];
int PE =0;
int PA = 0;
int CONNECTION_ACK_TOKEN =0;
int is_ini =0;


float t_perte = 0.7;
int index_f = 0;
char fenetre [TAILLEF]; //retien les pdu envoyé ou non (sous forme vrai ou faux)

/*
 *
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
    if(is_ini ==0){
        pthread_mutex_init(&wait_mutex, NULL);
        set_loss_rate(50);
        printf("loss rate mis \n");
        for (int i = 0; i <TAILLEF; i++){
            fenetre[i] = 1;
        }
        for(int i=0; i<MAX_SOCKET; i++){
            tab_sock[i].fd=-1;
        }
        is_ini =1;
    }
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   int result = initialize_components(sm); /* Appel obligatoire */
   if(result == -1){
        printf("erreur de initialize components\n");
   }
    compteur_socket++;
    if(compteur_socket>MAX_SOCKET){
        printf("Exception raised: too much socket\n");
        return -1;
    }
    int i =0;
    while(tab_sock[i].fd!=-1){
        i++;
        if(i>MAX_SOCKET){
            printf("Unexpected Exception : tab_sock full\n");
            return -1;
        }
    }
    mic_tcp_sock sock;
    sock.fd = i;
    tab_sock[i] = sock;
   return i;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    short port = addr.port;
    for(int i =0; i<MAX_SOCKET; i++){
        if(tab_sock[i].local_addr.port==port){
            printf("Port deja attribué \n");
            return -1;
        }
    }
    tab_sock[socket].local_addr = addr;
   return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    protocol_state* state=&tab_sock[socket].state;
    *state = IDLE;

    //Attente SYN
    //printf("[------------DEBUG----------] ATTENTE SYN...\n");
    pthread_cond_wait(&syn_condition, &wait_mutex);// ATTENTE SYN
    pthread_mutex_unlock(&wait_mutex);


    //Attente ACK
    //printf("[------------DEBUG----------] ATTENTE ACK... \n");
    pthread_cond_wait(&ack_condition, &wait_mutex);// ATTENTE ACK
    pthread_mutex_unlock(&wait_mutex);

    printf("[MIC_TCP_ACCEPT] CONNEXION ETABLIE ✅\n\n");
    printf("[----------DEBUG------------] PA : %d | PE : %d\n", PA,PE);
    return 0;
}
/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    // stock l'addresse et le port passé en paramettre dans la structure
    tab_sock[socket].remote_addr = addr;
    protocol_state* state=&tab_sock[socket].state;
    
    
    //Construction SYN
    mic_tcp_pdu syn;
    syn.header.seq_num = PE;
    printf("[------------DEBUG----------] SEQ_NUM envoie SYN : %d\n", syn.header.seq_num);
    syn.header.syn = 1;
    syn.header.ack = 0;
    syn.header.source_port = 6500;
    syn.header.dest_port = tab_sock[socket].remote_addr.port;
    syn.payload.data = malloc(8);
    syn.payload.size = 8;

    //Construction SYNACK
    mic_tcp_ip_addr* local_ip=malloc(sizeof(mic_tcp_ip_addr));
    local_ip->addr_size=0;
    mic_tcp_ip_addr* remote_ip=malloc(sizeof(mic_tcp_ip_addr));
    remote_ip->addr=malloc(16);
    remote_ip->addr_size=16;
    mic_tcp_pdu synack;
    synack.payload.data = malloc(8);
    synack.payload.size = 8; 
    while(*state!=ESTABLISHED){ 

        
        int size_sent = IP_send(syn,addr.ip_addr);
        if(size_sent == -1){perror("[MIC_TCP_CONNECT] ERREUR SEND SYN");
        }else if(*state==IDLE){ //CHANGEMENT D'ETAT A SYN_SENT POUR ATTENDRE LE SYNACK
            *state=SYN_SENT;
            PA=(PE+1); //%2;
        } 
        printf("[------------DEBUG----------] SYN ENVOYE !\n");

        unsigned long t1 = get_now_time_msec();
        while(t1-get_now_time_msec()<MAX_TIME_WAIT){ // ATTENTE SYNACK
            int size = IP_recv(&synack,local_ip,remote_ip,MAX_TIME_WAIT);
            if(size == -1){
                perror("erreur recv ");
            }
            else if(synack.header.ack && synack.header.syn ){ //&& synack.header.ack_num==PA
                    printf("[----------DEBUG------------] SEQ_NUM recep synack : %d\n", synack.header.seq_num);
                    PE=synack.header.ack_num;//(PE+1); //%2;
                    PA=synack.header.seq_num+1;
                    *state=ESTABLISHED;
                    break;
                
            }
        }
    }
    printf("[------------DEBUG----------] SYNACK RECU !\n");
    
    //Envoi dernier ack
    mic_tcp_pdu ack;
    ack.header.ack=1;
    ack.header.syn=0;
    ack.header.seq_num=PE;
    ack.header.ack_num=synack.header.seq_num+1; //synack.header.seq_num+1
    CONNECTION_ACK_TOKEN=ack.header.seq_num;
    ack.header.source_port = 6500;
    ack.header.dest_port = tab_sock[socket].remote_addr.port;
    ack.payload.data = malloc(8);
    ack.payload.size = 8;
    IP_send(ack,addr.ip_addr);
    printf("[------------DEBUG----------] ACK ENVOYE !\n");


    printf("[MIC_TCP_CONNECT] CONNEXION ETABLIE ✅\n\n"); //Non c'est pas chat gpt, je suis allé chercher cet emoji moi meme sur le net
    printf("[----------DEBUG------------] PA : %d | PE : %d\n", PA,PE);
    PA=PE+1;
    return 0;
}

/*
 * Verifie si le pourcentage d'erreur est acceptable
 * Retourne 0 s'il ne l'est pas, 1 sinon.
 */
char verificationFenetre(){
    int nbreussi = 0;
    for (int i = 0; i < TAILLEF; i++){
        nbreussi = nbreussi + fenetre[i];
    }
    float pourcenRecup = ((float)nbreussi)/((float)TAILLEF);
    printf("[MIC-TCP] Pourcentage perte sur fenetre glissante : %f \n",pourcenRecup);
    return (pourcenRecup>t_perte);
}



/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");


    //Construction Message
    mic_tcp_pdu pdu;
    pdu.header.ack=0;
    pdu.header.syn=0;
    pdu.header.ack_num=CONNECTION_ACK_TOKEN;
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;
    pdu.header.seq_num = PE;
    pdu.header.source_port = tab_sock[mic_sock].local_addr.port;
    pdu.header.dest_port = tab_sock[mic_sock].remote_addr.port;

    //Construction ACK
    mic_tcp_ip_addr local_ip;
    mic_tcp_ip_addr remote_ip;
    remote_ip.addr=malloc(16);
    remote_ip.addr_size=16;
    mic_tcp_pdu ack;
    ack.payload.data = malloc(8);
    ack.payload.size = 8;

    //phase d'attente
    int size_sent = -1;
    int done = 0;
    while(!done){
        //Envoyer un message (dont la taille et le contenu sont passé en paramettre)
        size_sent = IP_send(pdu,tab_sock[mic_sock].remote_addr.ip_addr);
        unsigned long t1 = get_now_time_msec();
        printf("[----------DEBUG------------] PA : %d | PE : %d\n", PA,PE);
        //Attente du Ack
        while(t1-get_now_time_msec()<MAX_TIME_WAIT){
            int size = IP_recv(&ack,&local_ip,&remote_ip,MAX_TIME_WAIT);
            if(size == -1){
                perror("erreur recv ");
            }
            else if(ack.header.ack && ack.header.seq_num==pdu.header.seq_num+1){
                    PE=(PE+1);
                    fenetre[index_f] = 1;
                    index_f = (index_f + 1)%TAILLEF;
                    done = 1;
                    break;
                
            }
        }
        if(!done){
            fenetre[index_f]=0;
            index_f = (index_f + 1)%TAILLEF;
            if(verificationFenetre()){
                done = 1;
                printf("[MIC-TCP] Paquet non renvoyé \n");
            }
        }
    }
    
    
    return size_sent;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size) 
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    

	mic_tcp_payload payload;
	payload.data = mesg;
	payload.size = max_mesg_size;
    int size_ret = -1;
    
    while(size_ret<1){
	    size_ret = app_buffer_get(payload);
    }
    return size_ret;
}


/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    if(socket>MAX_SOCKET || socket<0){
        printf("Exception : socket out of bound\n");
        return -1;
    }
	compteur_socket--;
    tab_sock[socket].fd=-1;
    return 1; 	
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    int sock=0;
    while(tab_sock[sock].local_addr.port!=pdu.header.dest_port){sock++;} //trouve le socket actuel dans le tableau de socket
    protocol_state* state=&tab_sock[sock].state;

    if( !pdu.header.syn && *state==SYN_RECEIVED && pdu.header.ack_num==PA){
            *state=ESTABLISHED;
            PE++;
            pthread_cond_signal(&ack_condition);
    }

    //[CONNEXION ETABLIE]
    if (*state==ESTABLISHED){
        mic_tcp_pdu ack;
        ack.header.ack=1;
        ack.header.seq_num = pdu.header.seq_num+1;
        IP_send(ack,remote_addr);
        if(pdu.header.seq_num==PA){
            PA++; 
            app_buffer_put(pdu.payload);
        }
        printf("[----------DEBUG------------] PA : %d | PE : %d\n", PA,PE);

    }else if (pdu.header.ack && pdu.header.syn){ //SYNACK
        printf("[Process_received_PDU] UN SYNACK ? DANS CETTE ECONOMIE ???\n");
    }else if (pdu.header.ack){ //ACK

        if(*state==SYN_RECEIVED && pdu.header.seq_num==PA){ //[PHASE DE CONNEXION] recep ACK   
            printf("[----------DEBUG------------] ACK_NUM recep ack : %d || %d | PA : %d\n", pdu.header.ack_num,pdu.header.seq_num,PA);
            *state=ESTABLISHED;
            PE++;
            pthread_cond_signal(&ack_condition);
        }

    }else if (pdu.header.syn){ //SYN

        if(*state==IDLE){//[PHASE DE CONNEXION] recep SYN
            *state=SYN_RECEIVED;
            pthread_cond_signal(&syn_condition);
            printf("[----------DEBUG------------] SEQ_NUM : recep syn %d\n", pdu.header.seq_num);
            PE=1000;
            PA=(pdu.header.seq_num+1);

            //Envoie SYN+ACK
            mic_tcp_pdu synack;
            synack.header.ack=1;
            synack.header.syn=1;
            synack.header.ack_num=pdu.header.seq_num+1; //%2;
            synack.header.seq_num=PE; //Normalement c'est le core qui le fait
            printf("[----------DEBUG------------] SEQ_NUM envoie synack : %d | ACK_num %d\n", synack.header.seq_num,synack.header.ack_num);
            synack.payload.data = malloc(8);
            synack.payload.size = 8;
            int size_sent = IP_send(synack,remote_addr);
            if (size_sent==-1){perror("[MIC_TCP_ACCEPT] ERREUR SEND SYNACK");}
            printf("[------------DEBUG----------] SYNACK ENVOYE !\n");

        }
    }
}


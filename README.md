# BE RESEAU INSA 3A-MIC 

## Compilation

    Entrez la commande:
    
        make

    *Un warning s'affiche à la compilation, il est necessaire pour l'intéractivité de notre programme.*

## Presentation générale du BE

Ce bureau d'étude vise à implémenter une version Stop & Wait du protocole TCP, sur la base d'un core donné par les enseigants.

## Versions et choix d'Implementation

### v1
La V1 permet un transfert simple d'information. On implémente une première version de toutes les fonctions que nous allons utiliser pendant le BE.

1. Un tableau pour le stockage des sockets
    Celui-ci nous a permis de garder les sockets nécessaires à portée de main dans une structure simple à manipuler.
    Un emplacement socket est à -1 s'il n'est pas utilisé, sinon il est égal à sa position dans le tableau.

2. La fonction mic_tcp_bind()
    Elle vérifie que le port auquel on souhaite lier le socket ne soit pas déjà utilisé par un autre socket.
    S'il est libre, elle le lie.

3. mic_tcp_accept()
    Le socket actif est mis en IDLE

4. mic_tcp_connect()
    On associe l'adresse distante du destinataire au socket.

5. mic_tcp_send()
    Création d'un PDU et envoie via IP_Send().

6. mic_tcp_recv()
    Creation d'un payload puis on boucle jusqu'a ce que app_buffer_get() recoive une info

7. mic_tcp_close()
    On repasse le socket en parametre a -1

8. Process_receive_pdu()
    app_buffer_put()

### v2
On ajoute une fiabilité totale via le mecanisme de Stop&Wait.

1. mic_tcp_send()
    Une boucle est ajoutée pour renvoyer le pdu tant qu'on a pas reçu un ack avec le bon numéro de séquence (ils alternent entre 0 ou 1 dans cette version). On utilise le timeout de la fonction IP_recv pour ne pas rester bloqué.

2. process_receive_PDU()
    A chaque reception on renvoie un pdu d'aquittement et on met à jour les numéros de séquence si le numéro de séquence recu est celui attendu. 


### v3
On implemente une fiabilité partielle avec un pourcentage de perte fixe.

Nous avons choisi d'utiliser une fenetre glissante via un tableau, si un pdu est envoyé la case actuelle est mise a 1, sinon a 0.
En faisant la somme du tableau on peut connaitre le pourcentage de perte dans la fenetre.
Cette méthode permet de ne pas reprendre des pertes occasionnelles et empeche qu'une vague de perte ne sois pas reprise (si tous les paquets  depuis le début de la connexion n'étaient pas perdus et qu'on calculait un pourcentage total par exemple).

### v4
 
1. Implémentation de la phase de connexion et asynchronisme

    Nous avons choisi de mettre les numéros de séquence dans la phase de connexion même si ceux-ci ne sont pas necessaires dans le cas ou le serveur ne se connecte qu'à 1 client.
    La phase de connexion coté client est gérée de manière synchrone avec deux IP_send et un IP_recv.
    La phase de connexion coté serveur est gérée asynchronement dans process_receive_PDU et mic_tcp_accept. La fonction mic_tcp_accept est bloquée à chaque étape par des variables de conditions et mutex en attendant que le changement de l'état de la connexion soit signalé par process_receive_PDU().

2. Negociation du taux de perte

    L'utilisateur entre le pourcentage de reprise qu'il souhaite coté client, et celui maximal que le serveur est capable de d'accepter.
    Le client envoie cette information dans le payload du SYN, si le serveur est capable d'accéder a la demande (taux de reprise pas trop élevé coté client), alors c'est le cas, sinon le serveur se met au max de ses capacités.
    Nous avons choisi d'utiliser une interaction utilisateur pour la simplicité des tests du BE.

## Discussion sur les performances de MIC_TCP_v4 , MIC_TCP_v2 et TCP

TCP implémente une fiabilité totale, très utiles pour certains usages mais peut s'avérer lents pour des utilisations en temps réel par exemple.
MIC_TCP_v2 a aussi une fiabilité totale, mais le mécanisme de stop&wait est très simple et le rend plus lent que TCP classique.

Concernant MIC_TCP_v4 il a l'avantage de ne pas considérer les pertes "acceptables". Dans le cas de la vidéo en temps réel par exemple s'il manque quelques images l'utilisateur n'en ressentira pas les conséquences, mais le cas d'un arret de l'image puis d'une acceleration lors de la récuperation de la connexion peu s'avérer génant. Le taux de reprise de MIC_TCP_v4 est negociable ce qui le rend utilisable dans de nombreux cas (même à 100%).


/**
 * @file    main.c
 * @author  Samandar Komil
 * @date    18 April 2025
 * 
 * @brief   Main file to run the program.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "Server.h"


int main(void){
    Server TCPServer = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, 8000, 10);
    TCPServer.launch(&TCPServer);

    return 0;
}
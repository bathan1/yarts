/**
 * @file tcp.h
 * @brief socket API 
 */
#pragma once
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/types.h>
#include <stdbool.h>

#define MAX_HOSTNAME_LENGTH 255

/**
 * @brief Write address info from HOSTNAME and PORT into ADDR.
 *
 * @retval 0 OK. Address Info written to ADDR.
 * @retval 22 EINVAL. At least one of HOSTNAME or PORT or ADDR is NULL.
 * @retval ANYTHING_ELSE Error. The return value of \c getaddrinfo() when non-zero.
 */
int tcp_getaddrinfo(const char *hostname, const char *port, struct addrinfo **addr);

/**
 * @brief Open socket fd from ADDRINFO and return it.
 *
 * @retval -1 Error opening socket.
 * @retval NONNEGATIVE OK, a socket file descriptor.
 */
int tcp_socket(struct addrinfo *addrinfo);

/**
 * @brief \c connect() to socket FD using ADDR, optionally using TLS
 * if TLS is not NULL.
 *
 * @retval 0 OK
 * @retval -1 Error connecting to socket
 * @retval -2 Error with TLS connection
 */
int tcp_connect(int fd, struct sockaddr * addr, socklen_t len,
                 SSL **ssl, SSL_CTX **ctx, const char *hostname);

/**
 * @brief Send LEN BYTES over tcp connection at FD, potentially writing
 * over the TLS connection at SSL, if it exists.
 *
 * If SSL is `NULL`, then #ttcp_send() fallsback to plain \c send() call to SOCKFD.
 *
 * @retval -1 ERROR - Neither the SOCKFD or SSL sockets can be written to.
 * @retval x>=0 OK - `x` bytes written out.
 */
ssize_t tcp_send(int fd, const char *bytes, size_t len, SSL *ssl);

/**
 * @brief Maybe recv LEN BYTES over the TCP connection at SSL, if it exists.
 *
 * If SSL is `NULL` then #ttcp_recv() fallsback to plain \c recv() call to SOCKFD.
 *
 * @retval -1 ERROR - Neither the SOCKFD or SSL sockets can be written to.
 * @retval (x >= 0) OK - X bytes written out.
 */
ssize_t tcp_recv(int fd, char *bytes, size_t len, SSL *ssl);

/**
 * @brief Shutdown and free SSL and CTX.
 *
 * If both SSL and CTX are NULL, then this is a no-op.
 */
void tcp_tls_free(SSL *ssl, SSL_CTX *ctx);

#undef MAX_HOSTNAME_LENGTH

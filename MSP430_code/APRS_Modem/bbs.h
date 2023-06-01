#ifndef BBS_H_
#define BBS_H_

struct message {
    char source[7];
    char dest[7];
    char payload[64];
};


#endif /* BBS_H_ */

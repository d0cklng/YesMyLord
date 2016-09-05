#ifndef JINPLATFORMGAP_H
#define JINPLATFORMGAP_H

#ifndef JWIN
extern char* itoa( int value, char* result, int base );
#endif

#if defined(JWIN) && !defined(JMINGW)
extern char *optarg;
extern int optind;
extern int opterr;
extern int getopt(int argc, char *argv[], char *opstring);
#endif


//class jinplatformgap
//{
//public:
//    jinplatformgap();
//};

#endif // JINPLATFORMGAP_H

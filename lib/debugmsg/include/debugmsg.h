#ifndef DEBUGMSG_H
#define DEBUGMSG_H

#ifdef AIR_QUALITY_LOG_DEBUG

#define DEBUGMSG(msg) do {				\
		printf("DEBUG: %s:L%u " msg "\n",	\
		       __FILE__,			\
		       (unsigned int) __LINE__);	\
	} while (0);

#define DEBUGDATA(msg, data, type) do {				\
		printf("DEBUG: %s:L%u " msg " " type "\n",	\
		       __FILE__,				\
		       (unsigned int) __LINE__, data);		\
	} while (0);

#else

#define DEBUGMSG(msg)
#define DEBUGDATA(msg, data, type)

#endif /* #ifdef AIR_QUALITY_DEBUG */

#endif /* #ifndef DEBUGMSG_H */

#ifndef __UMBO_DEBUG_H__
#define __UMBO_DEBUG_H__

#include <glib.h>
#include <unistd.h>

#define UMBO_LOG(LEVEL, MSG, args...) g_log (G_LOG_DOMAIN,         \
                                         LEVEL,                    \
                                         "(%s:%d)(%lu:%p):\t" MSG, \
                                         (char*)__FILE__,          \
                                         (int)__LINE__,            \
                                         (gulong)getpid(),         \
                                         (void *)g_thread_self(),  \
                                         ##args)


#define UMBO_DBG(MSG, args...)      UMBO_LOG(G_LOG_LEVEL_DEBUG,    MSG, ##args)
#define UMBO_INFO(MSG, args...)     UMBO_LOG(G_LOG_LEVEL_INFO,     MSG, ##args)
#define UMBO_MSG(MSG, args...)      UMBO_LOG(G_LOG_LEVEL_MESSAGE,  MSG, ##args)
#define UMBO_WARN(MSG, args...)     UMBO_LOG(G_LOG_LEVEL_WARNING,  MSG, ##args)
#define UMBO_CRITICAL(MSG, args...) UMBO_LOG(G_LOG_LEVEL_CRITICAL, MSG, ##args)
#define UMBO_ERR(MSG, args...)      UMBO_LOG(G_LOG_LEVEL_ERROR,    MSG, ##args)

#endif //__UMBO_DEBUG_H__

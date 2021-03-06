#pragma once

//base event polling
#include <EventPoller.h>

//std libraries
#include <string>
#include <stdint.h>
#include <sys/inotify.h>
#include <sys/epoll.h>

//boost unordered
#include <boost/unordered_map.hpp>

//Inotify constants
#define MAXPOLLSIZE 256
#define INOTIFY_EVENT_SIZE (sizeof (struct inotify_event))
#define INOTIFY_BUF_LEN (1024 * (INOTIFY_EVENT_SIZE + 16))

//Forward Declarations
class INotifyWatch;
class INotifyEventListener;

/** \brief INotify Event Poller.
 *         Detects for INotify events and performs the necessary
 *         Jobs defined by the user.
 *         Maintains a HashMap of all the current watch descriptors
 *         held by this INotifyPoller.
 */
class INotifyEventPoller : public EventPoller
{

public:
    INotifyEventPoller();
    ~INotifyEventPoller();

    //Override
    virtual int poll(int timeout);
    virtual int service();

    //Class specific functions to handle inotify
    int initWatch();
    int addWatch(const std::string& pathname, const uint32_t mask);
    int removeWatch(int wd);
    int removeWatch(const std::string& pathname);
    int removeAllWatch();
    bool addINotifyEventListener(int wd, INotifyEventListener* listener);
    bool removeINotifyEventListener(int wd, INotifyEventListener* listener);

protected:
private:

    //epoll
    int m_epoll_fd;
    struct epoll_event m_epoll_event;
    struct epoll_event m_epoll_events[MAXPOLLSIZE];
    int m_number_of_fds;

    //inotify
    int m_inotify_init_fd;
    char m_inotify_buf[INOTIFY_BUF_LEN];
    int m_inotify_buf_len;

    //wd to pathname map and maybe wd to Job in future
    boost::unordered_map<int, INotifyWatch*> m_watch_descriptors;
    boost::unordered_map<std::string, INotifyWatch*> m_watch_paths;
};

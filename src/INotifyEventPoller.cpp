#include <INotifyEventPoller.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

/** \brief Initialize the INotify on creations to
 *         begin accepting new watchers.
 */
INotifyEventPoller::INotifyEventPoller() :
    m_inotify_init_fd(0)
{
    initWatch();
}

/** \brief close the INotify instance
 */
INotifyEventPoller::~INotifyEventPoller()
{
    ///closing the INOTIFY instance
    removeAllWatch();
    close(m_inotify_init_fd);
    close(m_epoll_fd);
}

/** \brief Initialize an INotifyWatch.
 *
 * \return int On success, these system calls return a new file descriptor.
 *             On error, -1 is returned, and errno is set to indicate the error.
 *
 */
int INotifyEventPoller::initWatch()
{
    //check if an existing initialization descriptor is in use.
    //close it if present and clear the watchdescriptors associated with it.
    if(m_inotify_init_fd > 0)
    {
        //remove all watch descriptors
        removeAllWatch();
        close(m_inotify_init_fd);
        close(m_epoll_fd);
    }

    m_inotify_init_fd = inotify_init();
    if(m_inotify_init_fd < 0)
        perror("Error Initializing INotify");

    //setup the epoll events
    m_epoll_fd = epoll_create(MAXPOLLSIZE);
    m_epoll_event.events = EPOLLIN | EPOLLPRI;
    m_epoll_event.data.fd = m_inotify_init_fd;
    int res = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_inotify_init_fd, &m_epoll_event);
    if(res < 0)
        perror("Epoll Add Control");

    return m_inotify_init_fd;
}

/** \brief Adds a File or Folder to be watched.
 *
 * \param pathname const std::string&
 * \param mask uint32_t INotify Event Masks
 * \return int On success, inotify_add_watch() returns a nonnegative watch
 *              descriptor. On error, -1 is returned and errno is set appropriately.
 *
 *       EACCES Read access to the given file is not permitted.
 *       EBADF  The given file descriptor is not valid.
 *       EFAULT pathname points outside of the process's accessible address
 *              space.
 *       EINVAL The given event mask contains no valid events; or fd is not an
 *              inotify file descriptor.
 *       ENAMETOOLONG
 *              pathname is too long.
 *       ENOENT A directory component in pathname does not exist or is a
 *              dangling symbolic link.
 *       ENOMEM Insufficient kernel memory was available.
 *       ENOSPC The user limit on the total number of inotify watches was
 *              reached or the kernel failed to allocate a needed resource.
 */
int INotifyEventPoller::addWatch(const std::string& pathname, const uint32_t mask)
{
    int wd = inotify_add_watch(m_inotify_init_fd, pathname.c_str(), mask);

    if(wd < 0)
        perror("Unable to add new watch descriptors");
    else
    {
        m_watch_descriptors[wd] = pathname;
    }
    return wd;
}


/** \brief Removes a INotify watch using the Watch Descriptor return by addWatch.
 *         Removes the watch descriptor from the hashtable of watch descriptors
 *
 * \param wd int watch descriptor to unwatch
 * \return int On success, removeWatch() returns zero,
 *             or -1 if an error occurred (in which case, errno is set appropriately). <p>
 *
 *           EBADF
 *              fd is not a valid file descriptor.
 *           EINVAL
 *                The watch descriptor wd is not valid; or fd is not an inotify file descriptor.
 *
 */
int INotifyEventPoller::removeWatch(int wd)
{
    /*removing the “/tmp” directory from the watch list.*/
    int res = inotify_rm_watch(m_inotify_init_fd, wd);

    if(res < 0)
        perror("Unable to remove watch descriptor");
    else
    {
        m_watch_descriptors.erase(wd);
    }

    return res;
}

int INotifyEventPoller::removeAllWatch()
{
    for(boost::unordered_map<int, std::string>::iterator it = m_watch_descriptors.begin(), end = m_watch_descriptors.end();
    it != end;
    ++it)
    {
        int wd = (*it).first;
        inotify_rm_watch(m_inotify_init_fd, wd);
    }
    m_watch_descriptors.clear();
    return 0;
}

int INotifyEventPoller::poll(int timeout = -1)
{
    m_number_of_fds = epoll_wait(m_epoll_fd, m_epoll_events, MAXPOLLSIZE, timeout);
    return m_number_of_fds;
}

int INotifyEventPoller::service()
{
    int inotify_buf_len;

    if (m_number_of_fds < 0)
    {
        perror("epoll_wait");
        return 1;
    }

    for (int i = 0; i < m_number_of_fds; i++)
    {
        int fd = m_epoll_events[i].data.fd;

        if (fd == m_inotify_init_fd)
        {

            // it's an inotify thing
            inotify_buf_len = read (fd, m_inotify_buf, INOTIFY_BUF_LEN);
            int j=0;

            //process all the watch descriptors
            while ( j < inotify_buf_len)
            {
                struct inotify_event *event;

                event = (struct inotify_event *) &m_inotify_buf[j];

                if(event->len)
                {
                    if ( event->mask & IN_CREATE )
                    {
                        if ( event->mask & IN_ISDIR )
                        {
                            printf( "New directory %s created.\n", event->name );
                        }
                        else
                        {
                            printf( "New file %s created.\n", event->name );
                        }
                    }
                    else if ( event->mask & IN_DELETE )
                    {
                        if ( event->mask & IN_ISDIR )
                        {
                            printf( "Directory %s deleted.\n", event->name );
                        }
                        else
                        {
                            printf( "File %s deleted.\n", event->name );
                        }
                    }
                    else if ( event->mask & IN_MODIFY)
                    {
                        if( event->mask & IN_ISDIR)
                        {
                            printf( "Directory %s is modified\n", event->name);
                        }
                        else
                        {
                            printf( "File %s is modified\n", event->name);
                        }
                    }
                }

                printf ("wd=%d mask=%u cookie=%u len=%u\n", event->wd, event->mask, event->cookie, event->len);

                if (event->len) printf("name=%s\n", event->name);

                j += INOTIFY_EVENT_SIZE + event->len;

            }
        }
    }

    return 0;
}



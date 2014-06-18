#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/epoll.h>

#define MAXPOLLSIZE 256
#define INOTIFY_EVENT_SIZE (sizeof (struct inotify_event))
#define INOTIFY_BUF_LEN (1024 * (INOTIFY_EVENT_SIZE + 16))

#include <iostream>
#include <sstream>
#include <vector>
#include <map>

#include <boost/process.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/thread.hpp>

#include <INotifyEventPoller.h>
#include <INotifyEventListener.h>
#include <JobDispatcher.h>
#include <Job.h>

#define APPLICATION_NAME "Job Dispatcher"
#define VERSION_NUM "0.0.4"
#define COPYRIGHT "Copyright 2014 Immersive Labs. All rights reserved."

using namespace std;
using namespace boost::process;
using namespace boost::process::initializers;
using namespace imrsv;

//Simple Concrete INotifyEventListener
class AMSPostProcessor : public INotifyEventListener
{
public:

    class NoProgramConfigExistsException : public std::exception
    {
    public:
        virtual const char* what() const throw()
        {
            return "No Program Config Record Found!";
        }
    };

    AMSPostProcessor(JobDispatcher* dispatcher, const std::string& watch_directory) :
        m_dispatcher(dispatcher),
        m_watch_directory(watch_directory)
    {

    }

    void addProgram(const std::string& program_name, const boost::property_tree::ptree& config)
    {
        m_program_configs[program_name] = config;
    }

    void removeProgram(const std::string& program_name)
    {
        m_program_configs.erase(program_name);
    }

    const boost::property_tree::ptree& findProgram(const std::string& program_name)
    {
        std::map<std::string, boost::property_tree::ptree>::const_iterator found = m_program_configs.find(program_name);
        if(found == m_program_configs.end())
            throw NoProgramConfigExistsException();
        else
            return found->second;
    }

    const std::string& watch_directory() const
    {
        return m_watch_directory;
    }

    void setWatchDirectory(const std::string& directory)
    {
        m_watch_directory = directory;
    }

    void onReceiveINotifyEvent(const INotifyEvent& e)
    {
        std::cout << e << std::endl;
        std::string filename = e.name();

        try
        {
            //check for .ipg and .flv extensions
            if(filename.find(".ipg") != std::string::npos && filename.find(".flv") != std::string::npos)
            {

                const boost::property_tree::ptree& config = findProgram("ipg");
                std::string path = config.get<std::string>("path");

                Job j;
                //extract the filename without the extensions
                std::string::size_type pos = filename.find_first_of(".");
                std::string base_name = filename.substr(0, pos);

                std::string meta_data_file = base_name + ".meta";
                std::string output_file = base_name + ".output";

                //create a job for the job dispatcher
                Task t1a("/usr/local/bin/flvmeta", "-F -j " + filename, m_watch_directory);
                t1a.setRedirectStdOut(m_watch_directory + "/" + meta_data_file);
                Task t1b(path,
                         "-v " + filename + " -m " + meta_data_file + " -o " + output_file,
                         m_watch_directory);

                std::cout << t1a << std::endl;
                std::cout << t1b << std::endl;

                j.addTask(t1a);
                j.addTask(t1b);

                m_dispatcher->addJob(j);
            }
            if(filename.find(".emo.") != std::string::npos && filename.find(".flv") != std::string::npos)
            {
                std::ostringstream oss;
                const boost::property_tree::ptree& config = findProgram("emo");
                std::string path = config.get<std::string>("path");

                //emotion server arguments
                std::string database_data = config.get<std::string>("database_data","");
                std::string database_results = config.get<std::string>("database_results","");
                std::string collection_data = config.get<std::string>("collection_data","");
                std::string collection_results = config.get<std::string>("collection_results","");
                std::string bin_results_fieldname = config.get<std::string>("bin_results_fieldname","");
                bool realtime = config.get<bool>("realtime",false);
                bool print = config.get<bool>("print",false);
                bool print_raw = config.get<bool>("print_raw",false);
                bool print_time = config.get<bool>("print_time",false);
                int sampling_rate = config.get<int>("sampling",-1);
                bool verbose = config.get<bool>("verbose",false);

                //now we construct the arguments, remember to put spacing at end of each option
                std::string arguments = "";

                //look for db
                if(!database_data.empty())
                    arguments.append("--database_data=" + database_data + " ");

                if(!database_results.empty())
                    arguments.append("--database_results=" + database_results + " ");

                if(!collection_data.empty())
                    arguments.append("--collection_data=" + collection_data + " ");

                if(!collection_results.empty())
                    arguments.append("--collection_results=" + collection_results + " ");

                if(!bin_results_fieldname.empty())
                    arguments.append("--bin_results_fieldname=" + bin_results_fieldname + " ");

                if(realtime)
                    arguments.append("--realtime ");

                if(sampling_rate != -1)
                {
                    //get the sampling rate and conver to string
                    oss << sampling_rate;
                    arguments.append("--sampling_rate=" + oss.str() + " ");
                }

                if(print)
                    arguments.append("--print ");

                if(print_raw)
                    arguments.append("--print_raw ");

                if(print_time)
                    arguments.append("--print_time ");

                if(verbose)
                    arguments.append("--verbose ");

                Job j;
                //extract the filename without the extensions
                std::string::size_type pos = filename.find_first_of(".");
                std::string base_name = filename.substr(0, pos);
                std::string meta_data_file = base_name + ".meta";

                //mandatory arguments
                arguments.append("--insert ")
                .append(filename + " ")
                .append(meta_data_file);

                //create a job for the job dispatcher
                Task t1a("/usr/local/bin/flvmeta", "-F -j " + filename, m_watch_directory);
                t1a.setRedirectStdOut(m_watch_directory + "/" + meta_data_file);
                Task t1b(path, arguments, m_watch_directory);

                std::cout << t1a << std::endl;
                std::cout << t1b << std::endl;

                j.addTask(t1a);
                j.addTask(t1b);

                m_dispatcher->addJob(j);
            }
            if(filename.find(".emo-dev.") != std::string::npos && filename.find(".flv") != std::string::npos)
            {
                std::ostringstream oss;
                const boost::property_tree::ptree& config = findProgram("emo-dev");
                std::string path = config.get<std::string>("path");

                //emotion server arguments
                std::string database_data = config.get<std::string>("database_data","");
                std::string database_results = config.get<std::string>("database_results","");
                std::string collection_data = config.get<std::string>("collection_data","");
                std::string collection_results = config.get<std::string>("collection_results","");
                std::string bin_results_fieldname = config.get<std::string>("bin_results_fieldname","");
                bool realtime = config.get<bool>("realtime",false);
                bool print = config.get<bool>("print",false);
                bool print_raw = config.get<bool>("print_raw",false);
                bool print_time = config.get<bool>("print_time",false);
                int sampling_rate = config.get<int>("sampling",-1);
                bool verbose = config.get<bool>("verbose",false);

                //now we construct the arguments, remember to put spacing at end of each option
                std::string arguments = "";

                //look for db
                if(!database_data.empty())
                    arguments.append("--database_data=" + database_data + " ");

                if(!database_results.empty())
                    arguments.append("--database_results=" + database_results + " ");

                if(!collection_results.empty())
                    arguments.append("--collection_results=" + collection_results + " ");

                if(!bin_results_fieldname.empty())
                    arguments.append("--bin_results_fieldname=" + bin_results_fieldname + " ");

                if(realtime)
                    arguments.append("--realtime ");

                if(sampling_rate != -1)
                {
                    //get the sampling rate and conver to string
                    oss << sampling_rate;
                    arguments.append("--sampling_rate=" + oss.str() + " ");
                }

                if(print)
                    arguments.append("--print ");

                if(print_raw)
                    arguments.append("--print_raw ");

                if(print_time)
                    arguments.append("--print_time ");

                if(verbose)
                    arguments.append("--verbose ");

                Job j;
                //extract the filename without the extensions
                std::string::size_type pos = filename.find_first_of(".");
                std::string base_name = filename.substr(0, pos);
                std::string meta_data_file = base_name + ".meta";

                //mandatory arguments
                arguments.append("--insert ")
                .append(filename + " ")
                .append(meta_data_file);

                //create a job for the job dispatcher
                Task t1a("/usr/local/bin/flvmeta", "-F -j " + filename, m_watch_directory);
                t1a.setRedirectStdOut(m_watch_directory + "/" + meta_data_file);
                Task t1b(path, arguments, m_watch_directory);

                std::cout << t1a << std::endl;
                std::cout << t1b << std::endl;

                j.addTask(t1a);
                j.addTask(t1b);

                m_dispatcher->addJob(j);
            }
            if(filename.find(".ipg-dev.") != std::string::npos && filename.find(".flv") != std::string::npos)
            {
                std::ostringstream oss;
                const boost::property_tree::ptree& config = findProgram("ipg-dev");
                std::string path = config.get<std::string>("path");

                //emotion server arguments
                std::string database_data = config.get<std::string>("database_data","");
                std::string database_results = config.get<std::string>("database_results","");
                std::string collection_data = config.get<std::string>("collection_data","");
                std::string collection_results = config.get<std::string>("collection_results","");
                std::string bin_results_fieldname = config.get<std::string>("bin_results_fieldname","");
                bool realtime = config.get<bool>("realtime",false);
                bool print = config.get<bool>("print",false);
                bool print_raw = config.get<bool>("print_raw",false);
                bool print_time = config.get<bool>("print_time",false);
                int sampling_rate = config.get<int>("sampling",-1);
                bool verbose = config.get<bool>("verbose",false);

                //now we construct the arguments, remember to put spacing at end of each option
                std::string arguments = "";

                //look for db
                if(!database_data.empty())
                    arguments.append("--database_data=" + database_data + " ");

                if(!database_results.empty())
                    arguments.append("--database_results=" + database_results + " ");

                if(!collection_results.empty())
                    arguments.append("--collection_results=" + collection_results + " ");

                if(!bin_results_fieldname.empty())
                    arguments.append("--bin_results_fieldname=" + bin_results_fieldname + " ");

                if(realtime)
                    arguments.append("--realtime ");

                if(sampling_rate != -1)
                {
                    //get the sampling rate and conver to string
                    oss << sampling_rate;
                    arguments.append("--sampling_rate=" + oss.str() + " ");
                }

                if(print)
                    arguments.append("--print ");

                if(print_raw)
                    arguments.append("--print_raw ");

                if(print_time)
                    arguments.append("--print_time ");

                if(verbose)
                    arguments.append("--verbose ");

                Job j;
                //extract the filename without the extensions
                std::string::size_type pos = filename.find_first_of(".");
                std::string base_name = filename.substr(0, pos);
                std::string meta_data_file = base_name + ".meta";

                //mandatory arguments
                arguments.append("--insert ")
                .append(filename + " ")
                .append(meta_data_file);

                //create a job for the job dispatcher
                Task t1a("/usr/local/bin/flvmeta", "-F -j " + filename, m_watch_directory);
                t1a.setRedirectStdOut(m_watch_directory + "/" + meta_data_file);
                Task t1b(path, arguments, m_watch_directory);

                std::cout << t1a << std::endl;
                std::cout << t1b << std::endl;

                j.addTask(t1a);
                j.addTask(t1b);

                m_dispatcher->addJob(j);
            }
        }
        catch (boost::property_tree::ptree_error& e)
        {
            std::cerr << e.what() << std::endl;
        }
        catch (NoProgramConfigExistsException& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

private:
    JobDispatcher* m_dispatcher;
    std::string m_watch_directory;
    std::map<std::string, boost::property_tree::ptree> m_program_configs;
};

int main(int argc, char* argv[])
{
    namespace po = boost::program_options;
    po::options_description description("Usage: job-dispatcher --config=[FILE]");

    //add program options
    description.add_options()
    ("help", "Display this help message")
    ("version", "Display the version number")
    ("config", po::value<std::string>(), "Configuration File");

    //parse command line
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << APPLICATION_NAME " - Asynchronous Job Dispatcher using Thread Pools and Boost Process" << std::endl
                  << description << std::endl;
        return 0;
    }

    if(vm.count("version"))
    {
        std::cout << APPLICATION_NAME << " " << VERSION_NUM << std::endl;
        std::cout << COPYRIGHT << std::endl;
        return 0;
    }

    //config file name
    std::string config_file;
    if(vm.count("config"))
        config_file = vm["config"].as<std::string>();
    else
    {
        std::cout << APPLICATION_NAME " - Asynchronous Job Dispatcher using Thread Pools and Boost Process" << std::endl
                  << description << std::endl;
        return 0;
    }

    //parse the json config file to obtain configuration
    int num_workers;
    int max_job_queue;
    std::string ipg_watch_dir;
    std::string ipg_watch_event;

    //property trees
    boost::property_tree::ptree pt;
    boost::property_tree::ptree dispatcher_conf;
    boost::property_tree::ptree ipg_old_conf;
    boost::property_tree::ptree emo_conf;
    boost::property_tree::ptree emo_dev_conf;
    boost::property_tree::ptree ipg_dev_conf;

    try
    {
        boost::property_tree::read_json(config_file, pt);

        dispatcher_conf = pt.get_child("dispatcher");
        ipg_old_conf = pt.get_child("ipg");
        emo_conf = pt.get_child("emo");
        emo_dev_conf = pt.get_child("emo-dev");
        ipg_dev_conf = pt.get_child("ipg-dev");

        num_workers = dispatcher_conf.get<int>("num_workers",boost::thread::hardware_concurrency() - 1);
        max_job_queue = dispatcher_conf.get<int>("max_job_queue");

        ipg_watch_dir = ipg_old_conf.get<std::string>("watch_directory");
        ipg_watch_event = ipg_old_conf.get<std::string>("watch_event");

        std::cout << num_workers << std::endl;
        std::cout << max_job_queue << std::endl;
        std::cout << ipg_watch_dir << std::endl;

    }
    catch (boost::property_tree::json_parser::json_parser_error& e)
    {
        e.what();
        std::terminate();
    }
    catch (boost::property_tree::ptree_error& e)
    {
        e.what();
        std::terminate();
    }

    JobDispatcher dispatcher(num_workers,max_job_queue);
    INotifyEventPoller inotify_poller;

    AMSPostProcessor* ams = new AMSPostProcessor(&dispatcher, ipg_watch_dir);

    //add the programs that ams will run
    ams->addProgram("emo", emo_conf);
    ams->addProgram("emo-dev", emo_dev_conf);
    ams->addProgram("ipg", ipg_old_conf);
    ams->addProgram("ipg-dev", ipg_dev_conf);

    uint32_t event_mask = INotifyEvent::parseEventMask(ipg_watch_event);
    int wd = inotify_poller.addWatch(ipg_watch_dir, event_mask);
    inotify_poller.addINotifyEventListener(wd,ams);

    while(1)
    {
        if(inotify_poller.poll(-1) > 0)
            inotify_poller.service();
    }
}

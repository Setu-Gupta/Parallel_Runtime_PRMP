#include <chrono>
#include "cpucounters.h" // PCM related: https://github.com/intel/pcm
#include "utils.h"       // PCM related: https://github.com/intel/pcm

#define millisleep(a) boost::this_fiber::sleep_for(std::chrono::milliseconds(a))

namespace logger {
        static pcm::PCM *___pcm;
        pcm::SystemCounterState ___before_sstate, ___after_sstate;
        double start_time;
        static int init=0;

        void start() {
                /////////////////////////////////////////////////////////////////////////
                ////////// START INITIALIZATION ----- MUST BE DONE ONLY ONCE ////////////
                /////////////////////////////////////////////////////////////////////////
                if(init) return;
                ___pcm = pcm::PCM::getInstance();
                if (init==0) {
                        ___pcm->resetPMU();
                        init=1;
                }
                // program() creates common semaphore for the singleton, so ideally to be called before any other references to PCM
                pcm::PCM::ErrorCode status = ___pcm->program();

                switch (status)
                {
                        case pcm::PCM::Success:
                                //std::cerr << "Success initializing PCM" << std::endl;
                                break;
                        case pcm::PCM::MSRAccessDenied:
                                std::cerr << "Access to Processor Counter Monitor has denied (no MSR or PCI CFG space access)." << std::endl;
                                exit(EXIT_FAILURE);
                        case pcm::PCM::PMUBusy:
                                std::cerr << "Access to Processor Counter Monitor has denied (Performance Monitoring Unit is occupied by other application). Try to stop the application that uses PMU." << std::endl;
                                std::cerr << "Alternatively you can try running PCM with option -r to reset PMU configuration at your own risk." << std::endl;
                                exit(EXIT_FAILURE);
                        default:
                                std::cerr << "Access to Processor Counter Monitor has denied (Unknown error)." << std::endl;
                                exit(EXIT_FAILURE);
                }
                /////////////////////////////////////////////////////////////////////////
                ////////// END OF INITIALIZATION ----- MUST BE DONE ONLY ONCE ///////////
                /////////////////////////////////////////////////////////////////////////
                auto time_now = std::chrono::system_clock::now();
                auto seconds = std::chrono::duration<double>(time_now.time_since_epoch());
                start_time = seconds.count();
                ___before_sstate = pcm::getSystemCounterState();
        }
        
        double get_jpi()
        {
                ___after_sstate = pcm::getSystemCounterState();
                double _joules = getConsumedJoules(___before_sstate, ___after_sstate);
                uint64_t instrtd = getInstructionsRetired(___before_sstate, ___after_sstate);
                ___before_sstate = pcm::getSystemCounterState();
                return _joules / instrtd;
        }

        void end() {
                ___after_sstate = pcm::getSystemCounterState();
                auto time_now = std::chrono::system_clock::now();
                auto seconds = std::chrono::duration<double>(time_now.time_since_epoch());
                double end = seconds.count();
                double _duration = end - start_time;
                double _joules = getConsumedJoules(___before_sstate, ___after_sstate);
                uint64_t instrtd = getInstructionsRetired(___before_sstate, ___after_sstate);
                uint64_t cycles = getCycles(___before_sstate, ___after_sstate);
                ___pcm->cleanup();
                std::cout<<"STATISTICS: Time = " <<_duration*1000<<" ms\n";
                std::cout<<"STATISTICS: Energy = " <<_joules<<" joules\n";
                std::cout<<"STATISTICS: EDP = " <<_joules*_duration<<" (Lower the Better)\n";
                std::cout<<"STATISTICS: JPI = " << _joules / instrtd<< "\n";
                std::cout<<"STATISTICS: IPC = " << (1.0f) * instrtd / cycles<< "\n";
        }
} //namespace logger


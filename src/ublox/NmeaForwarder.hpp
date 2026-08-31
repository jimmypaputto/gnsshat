/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_NMEA_FORWARDER_HPP_
#define JIMMY_PAPUTTO_NMEA_FORWARDER_HPP_

#include <string>
#include <thread>
#include <memory>
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>

#include "Gnss.hpp"
#include "Navigation.hpp"
#include "common/Notifier.hpp"


namespace JimmyPaputto
{

class NmeaForwarder
{
public:
    explicit NmeaForwarder();
    ~NmeaForwarder();

    bool createVirtualTty();
    void startForwarding(const Gnss& gnss, Notifier& navigationNotifier);
    void stopForwarding();
    void joinForwarding();

    std::string getDevicePath() const { return devicePath_; }
    bool isRunning() const;

private:
    void forwardingThread(const Gnss& gnss, Notifier& navigationNotifier,
                          std::stop_token stoken);
    std::string generateNmeaGGA(const Navigation& navigation);
    std::string generateNmeaRMC(const Navigation& navigation);
    std::string generateNmeaGSA(const Navigation& navigation);
    std::string generateNmeaGSV(const Navigation& navigation);
    std::string generateNmeaGST(const Navigation& navigation);
    std::string generateNmeaZDA(const Navigation& navigation);
    std::string calculateNmeaChecksum(const std::string& sentence);
    std::string formatLatitude(const double lat);
    std::string formatLongitude(const double lon);
    std::string formatTime(const Navigation& navigation);
    std::string formatDate(const Navigation& navigation);

    int masterFd_;
    int slaveFd_;
    std::string devicePath_;
    std::jthread forwardingThread_;

    std::string lastEpoch_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_NMEA_FORWARDER_HPP_

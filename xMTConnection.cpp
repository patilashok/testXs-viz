
#include "xMTConnection.h"


WirelessMasterCallback wirelessMasterCallback; // Callback for wireless master
std::vector<MtwCallback*> mtwCallbacks; // Callbacks for mtw devices

XsDevicePtr wirelessMasterDevice;

const int desiredUpdateRate = 75;	// Use 75 Hz update rate for MTWs
const int desiredRadioChannel = 19;	// Use radio channel 19 for wireless master.

xMTConnection::xMTConnection(QObject *parent):QObject(parent), stopped(false),running(false)
{
	//isRunnung = 0;
}

xMTConnection::~xMTConnection()
{
	//KillMTWConnection();
}

void xMTConnection::startmtw()
{
	isStopped = false;
	isRunnung = true;

	emit running();	
	MTWData();
}


/*! \brief Stream insertion operator overload for XsPortInfo */
std::ostream& operator << (std::ostream& out, XsPortInfo const & p)
{
	out << "Port: " << std::setw(2) << std::right << p.portNumber() << " (" << p.portName().toStdString() << ") @ "
		<< std::setw(7) << p.baudrate() << " Bd"
		<< ", " << "ID: " << p.deviceId().toString().toStdString()
		;
	return out;
}

/*! \brief Stream insertion operator overload for XsDevice */
std::ostream& operator << (std::ostream& out, XsDevice const & d)
{
	out << "ID: " << d.deviceId().toString().toStdString() << " (" << d.productCode().toStdString() << ")";
	return out;
}

/*! \brief Given a list of update rates and a desired update rate, returns the closest update rate to the desired one */
int xMTConnection::findClosestUpdateRate(const XsIntArray& supportedUpdateRates, const int desiredUpdateRate)
{
	if (supportedUpdateRates.empty())
	{
		return 0;
	}

	if (supportedUpdateRates.size() == 1)
	{
		return supportedUpdateRates[0];
	}

	int uRateDist = -1;
	int closestUpdateRate = -1;
	for (XsIntArray::const_iterator itUpRate = supportedUpdateRates.begin(); itUpRate != supportedUpdateRates.end(); ++itUpRate)
	{
		const int currDist = std::abs(*itUpRate - desiredUpdateRate);

		if ((uRateDist == -1) || (currDist < uRateDist))
		{
			uRateDist = currDist;
			closestUpdateRate = *itUpRate;
		}
	}
	return closestUpdateRate;
}

void xMTConnection::MTWData()
{
	emit SignalToObj_mainThread();
	//qDebug("Thread id %d inside mtw", (int)QThread::currentThreadId());

	if (!isRunnung || isStopped) return;

	xmtConnect();

	while (isRunnung)
	{
		GetMTWdata();

		//emit signalValueUpdate(string);
	}

	QMetaObject::invokeMethod(this, "MTWData", Qt::QueuedConnection);

}

bool xMTConnection:: xmtConnect()
{
	std::cout << "Constructing XsControl..." << std::endl;
	XsControl* control = XsControl::construct();
	if (control == 0)
	{
		std::cout << "Failed to construct XsControl instance." << std::endl;
	}

	try
	{
		std::cout << "Scanning ports..." << std::endl;
		XsPortInfoArray detectedDevices = XsScanner::scanPorts();


		XsPortInfoArray::const_iterator wirelessMasterPort = detectedDevices.begin();
		while (wirelessMasterPort != detectedDevices.end() && !wirelessMasterPort->deviceId().isWirelessMaster())
		{
			++wirelessMasterPort;
		}
		if (wirelessMasterPort == detectedDevices.end())
		{
			throw std::runtime_error("No wireless masters found");
		}


		std::cout << "Opening port..." << std::endl;
		if (!control->openPort(wirelessMasterPort->portName().toStdString(), wirelessMasterPort->baudrate()))
		{
			std::ostringstream error;
			error << "Failed to open port " << *wirelessMasterPort;
			throw std::runtime_error(error.str());
		}

		wirelessMasterDevice = control->device(wirelessMasterPort->deviceId());
		if (wirelessMasterDevice == 0)
		{
			std::ostringstream error;
			error << "Failed to construct XsDevice instance: " << *wirelessMasterPort;
			throw std::runtime_error(error.str());
		}

		std::cout << "XsDevice connected @ " << *wirelessMasterDevice << std::endl;

		if (!wirelessMasterDevice->gotoConfig())
		{
			std::ostringstream error;

			throw std::runtime_error(error.str());
		}


		wirelessMasterDevice->addCallbackHandler(&wirelessMasterCallback);


		const XsIntArray supportedUpdateRates = wirelessMasterDevice->supportedUpdateRates();


		for (XsIntArray::const_iterator itUpRate = supportedUpdateRates.begin(); itUpRate != supportedUpdateRates.end(); ++itUpRate)
		{
		std::cout << *itUpRate << " ";
		}
		std::cout << std::endl;

		const int newUpdateRate = findClosestUpdateRate(supportedUpdateRates, desiredUpdateRate);


		if (!wirelessMasterDevice->setUpdateRate(newUpdateRate))
		{
			std::ostringstream error;
			//error << "Failed to set update rate: " << *wirelessMasterDevice;
			throw std::runtime_error(error.str());
		}

		if (wirelessMasterDevice->isRadioEnabled())
		{
			if (!wirelessMasterDevice->disableRadio())
			{
				std::ostringstream error;
				error << "Failed to disable radio channel: " << *wirelessMasterDevice;
				throw std::runtime_error(error.str());
			}
		}

		//std::cout << "Setting radio channel to " << desiredRadioChannel << " and enabling radio..." << std::endl;
		if (!wirelessMasterDevice->enableRadio(desiredRadioChannel))
		{
			std::ostringstream error;
			//error << "Failed to set radio channel: " << *wirelessMasterDevice;
			throw std::runtime_error(error.str());
		}

		std::cout << "Waiting for MTW to wirelessly connect...\n" << std::endl;

		bool waitForConnections = true;
		size_t connectedMTWCount = wirelessMasterCallback.getWirelessMTWs().size();
		do
		{
			XsTime::msleep(100);

			while (true)
			{
				size_t nextCount = wirelessMasterCallback.getWirelessMTWs().size();
				if (nextCount != connectedMTWCount)
				{
					std::cout << "Number of connected MTWs: " << nextCount << std::endl;
					connectedMTWCount = nextCount;
					isRunnung = 1;
					waitForConnections = false;
				}
				else
				{
					break;
				}
			}
			
			/*if (_kbhit())
			{
				waitForConnections = (toupper((char)_getch()) != 'Y');
			}*/

			
		} while (waitForConnections);

		/*size_t nextCount = wirelessMasterCallback.getWirelessMTWs().size();

				if (nextCount > 0)
				{
					std::cout << "Number of connected MTWs: " << connectedMTWCount << std::endl;
					isRunnung = 1;					
				}
				else
				{
					std::ostringstream error;
					error << "Failed to goto measurement mode: " << *wirelessMasterDevice;
					throw std::runtime_error(error.str());
					isRunnung = 0;
					return false;
				}*/
		
			

		std::cout << "Starting measurement..." << std::endl;
		if (!wirelessMasterDevice->gotoMeasurement())
		{
			std::ostringstream error;
			error << "Failed to goto measurement mode: " << *wirelessMasterDevice;
			throw std::runtime_error(error.str());
		}

		//std::cout << "Getting XsDevice instances for all MTWs..." << std::endl;
		XsDeviceIdArray allDeviceIds = control->deviceIds();
		XsDeviceIdArray mtwDeviceIds;
		for (XsDeviceIdArray::const_iterator i = allDeviceIds.begin(); i != allDeviceIds.end(); ++i)
		{
			if (i->isMtw())
			{
				mtwDeviceIds.push_back(*i);
			}
		}
		XsDevicePtrArray mtwDevices;
		for (XsDeviceIdArray::const_iterator i = mtwDeviceIds.begin(); i != mtwDeviceIds.end(); ++i)
		{
			XsDevicePtr mtwDevice = control->device(*i);
			if (mtwDevice != 0)
			{
				mtwDevices.push_back(mtwDevice);
			}
			else
			{
				throw std::runtime_error("Failed to create an MTW XsDevice instance");
			}
		}


		mtwCallbacks.resize(mtwDevices.size());
		for (int i = 0; i < (int)mtwDevices.size(); ++i)
		{
			mtwCallbacks[i] = new MtwCallback(i, mtwDevices[i]);
			mtwDevices[i]->addCallbackHandler(mtwCallbacks[i]);
		}
				
		return true;
	}
	catch (std::exception const & ex)
	{
		std::cout << ex.what() << std::endl;
	}
	catch (...)
	{
		std::cout << "An unknown fatal error has occured. Aborting." << std::endl;
	}

	std::cout << "Closing XsControl..." << std::endl;
	control->close();

	////std::cout << "Deleting mtw callbacks..." << std::endl;
	//for (std::vector<MtwCallback*>::iterator i = mtwCallbacks.begin(); i != mtwCallbacks.end(); ++i)
	//{
	//	delete (*i);
	//}

	//std::cout << "Successful exit." << std::endl;
	std::cout << "Press [ENTER] to continue." << std::endl; std::cin.get();
}

void xMTConnection::KillMTWConnection()
{

	isRunnung = false;
	isStopped = true;

	//std::cout << "Deleting mtw callbacks..." << std::endl;
	//std::cout << "Setting config mode..." << std::endl;
	if (!wirelessMasterDevice->gotoConfig())
	{
		std::ostringstream error;
		error << "Failed to goto config mode: " << *wirelessMasterDevice;
		throw std::runtime_error(error.str());
	}

	std::cout << "Disabling radio... " << std::endl;
	if (!wirelessMasterDevice->disableRadio())
	{
		std::ostringstream error;
		error << "Failed to disable radio: " << *wirelessMasterDevice;
		throw std::runtime_error(error.str());
	}

	for (std::vector<MtwCallback*>::iterator i = mtwCallbacks.begin(); i != mtwCallbacks.end(); ++i)
	{
		delete (*i);
	}

	std::cout << "Successful exit." << std::endl;

}

bool xMTConnection::UpdateMTWData(double &qRoll, double &qPitch, double &qYaw)
{
	qRoll = mtRoll; qPitch = mtPitch; qYaw = mtYaw;

	return isRunnung;

}

void xMTConnection::GetMTWdata()
{
	/*if (xmtConnect())
	{*/
		unsigned int printCounter = 0;
	std::vector<XsQuaternion> quaterdata(mtwCallbacks.size());
	//std::vector<XsEuler> eulerData(mtwCallbacks.size()); // Room to store euler data for each mtw
	//unsigned int printCounter = 0;
	//while (!_kbhit()) {
		XsTime::msleep(0);

		bool newDataAvailable = false;
		for (size_t i = 0; i < mtwCallbacks.size(); ++i)
		{
			if (mtwCallbacks[i]->dataAvailable())
			{
				newDataAvailable = true;
				XsDataPacket const * packet = mtwCallbacks[i]->getOldestPacket();
				//eulerData[i] = packet->orientationEuler();
				quaterdata[i] = packet->orientationQuaternion();
				mtwCallbacks[i]->deleteOldestPacket();
			}
		}

		if (newDataAvailable)
		{
			// Don't print too often for performance. Console output is very slow.
			//if (printCounter % 5 == 0)
			{
				for (size_t i = 0; i < mtwCallbacks.size(); ++i)
				{
					/*std::cout << "[" << i << "]: ID: " << mtwCallbacks[i]->device().deviceId().toString().toStdString()
					<< ", Roll: " << std::setw(7) << std::fixed << std::setprecision(2) << eulerData[i].roll()
					<< ", Pitch: " << std::setw(7) << std::fixed << std::setprecision(2) << eulerData[i].pitch()
					<< ", Yaw: " << std::setw(7) << std::fixed << std::setprecision(2) << eulerData[i].yaw()
					<< "\n";*/
					double Qroll = atan2((2 * quaterdata[i].y()*quaterdata[i].w()) + (2 * quaterdata[i].x()*quaterdata[i].z()),
						(1 - 2 * quaterdata[i].y() * quaterdata[i].y()) - (2 * quaterdata[i].z()*quaterdata[i].z()));
					double Qpitch = atan2((2 * quaterdata[i].x()*quaterdata[i].w()) + (2 * quaterdata[i].y()*quaterdata[i].z()),
						(1 - 2 * quaterdata[i].x()*quaterdata[i].x()) - (2 * quaterdata[i].z()*quaterdata[i].z()));
					double Qyaw = asin(2 * quaterdata[i].x()*quaterdata[i].y()) + (2 * quaterdata[i].z()*quaterdata[i].w());

					std::cout << "[" << i << "]: ID: " << mtwCallbacks[i]->device().deviceId().toString().toStdString()
						<< ", Roll: " << std::setw(7) << std::fixed << std::setprecision(2) << Qroll
						<< ", Pitch: " << std::setw(7) << std::fixed << std::setprecision(2) << Qpitch
						<< ", Yaw: " << std::setw(7) << std::fixed << std::setprecision(2) << Qyaw
						<< "\n";

					mtRoll = Qroll; mtPitch = Qpitch; mtYaw = Qyaw;
				}
			}
			//++printCounter;
		}

	//} std::cout << "Connection Failed" << std::endl;
	
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2023, Consolinno Energy GmbH
* Contact: office@consolinno.de
*
* GNU Lesser General Public License Usage
* This project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "integrationplugingenericconsolinno.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <network/networkaccessmanager.h>
#include <plugintimer.h>
#include <QtNetwork>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QTimer>
#include <QString>
#include <QNetworkAccessManager>
#include <QtCore/qglobal.h>
#include <QtCore/QRandomGenerator>
#include <iostream>
#include <limits>

IntegrationPluginGenericConsolinno::IntegrationPluginGenericConsolinno()
{

}

void IntegrationPluginGenericConsolinno::init()
{
	// Initialisation can be done here.
    qCDebug(dcGenericConsolinno()) << "Plugin initialized.";
    // qCWarning(dcGenericConsolinno()) << "Plugin initialized. qCWarning.";
}

void IntegrationPluginGenericConsolinno::discoverThings(ThingDiscoveryInfo *info)
{
    //Init QT jsonrpc client, call method: discoverDevice with param identifier: ""
	qCDebug(dcGenericConsolinno()) << "Discovering Generic Consolinno Inverters";

	// Search for devices in the network
	NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
	connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=]() {
		qCDebug(dcGenericConsolinno()) << "Finished network discovery";
		// qCWarning(dcGenericConsolinno()) << "network cache " << hardwareManager()->networkDeviceDiscovery()->cache();

		// Get the nymea cache of all found devices
		QHash<MacAddress, NetworkDeviceInfo> deviceCache = hardwareManager()->networkDeviceDiscovery()->cache();
		Json::Value discoveredIpAddresses(Json::arrayValue);

		// Write the ip addresses of the discovered devices into a JsonArray
		for(QHash<MacAddress, NetworkDeviceInfo>::const_iterator it=deviceCache.cbegin(); it!=deviceCache.cend(); ++it)
		{
			// qCWarning(dcGenericConsolinno()) << "IP address " << it.value().address().toQString().toStdString(); 
			discoveredIpAddresses.append(it.value().address().toString().toStdString());;
		}

		HttpClient client(urlModbusRTU);
		// Set the timeout for the curl command in msec.
		// If the timeout is set too low, curl will timeout before all devices have been checked and nymea will report,
		// that no devices have been found.
		client.SetTimeout(30000);
		Client c(client);

		Json::Value params;
		params["identifier"] = "";
		params["discoveredIpAddresses"] = discoveredIpAddresses;

		//Json::StreamWriterBuilder writer;
		//std::string output = Json::writeString(writer, params);
		//std::cout << output << std::endl;

		try {
			//Save c.CallMethod("discoverAllDevices", params); responce to json object
			Json::Value result = c.CallMethod("discoverAllDevices", params);
			//iterate on result
			std::cout << result << result.size() << std::endl;
			for (Json::Value::ArrayIndex i = 0; i < result.size(); i++) {
				//Debug out result[i]
				// std::cout << "result[" << i << "]:" << result[i] << std::endl;
				//Call processDiscoveryObject with result[i] and info
				processDiscoveryObject(result[i], info);
			}
			info->finish(Thing::ThingErrorNoError);
		} catch (JsonRpcException &e) {
			// std::cerr << e.what() << std::endl;
			info->finish(Thing::ThingErrorHardwareFailure);
		}
	});

}

void IntegrationPluginGenericConsolinno::processDiscoveryObject(const Json::Value &responseObject, ThingDiscoveryInfo *info){
	ThingDescriptor descriptor(
		genericConsolinnoConnectionThingClassId, 
		QString::fromStdString(responseObject["manufacturer"].asString()), 
		QString::fromStdString(
			"Model: " + responseObject["model"].asString() + 
			", Interface: " + responseObject["interface"].asString() + 
			", Slave: " + responseObject["slaveId"].asString())
	);
	ParamList params;
	params << Param(genericConsolinnoConnectionThingConnectionParamsParamTypeId, QVariant::fromValue((QString::fromStdString(responseObject["connectionParams"].asString()))));
	descriptor.setParams(params);
	info->addThingDescriptor(descriptor);
}

void IntegrationPluginGenericConsolinno::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcGenericConsolinno()) << "Setup" << thing << thing->params();
	m_timer = hardwareManager()->pluginTimerManager()->registerTimer(1);

    if (thing->thingClassId() == genericConsolinnoConnectionThingClassId) {

		// read out values
		connect(m_timer, &PluginTimer::timeout, thing, [this, thing](){
			QString connectionParams = thing->paramValue(genericConsolinnoConnectionThingConnectionParamsParamTypeId).toString();
			// qCDebug(dcGenericConsolinno()) << "connectionParams:" << connectionParams;
			//Jsonrpc request to get currentData
			HttpClient client(urlModbusRTU);
			Client c(client);
			Json::Value params;
			params["connectionParams"] = connectionParams.toStdString();
			params["block"] = -1;
			Things inverterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(inverterThingClassId);
			Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(meterThingClassId);
			Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(batteryThingClassId);

			bool inverterDataFound = false;
			bool meterDataFound = false;
			bool batteryDataFound = false;
			
			try {
				//Save c.CallMethod("discoverAllDevices", params); responce to json object
				Json::Value result = c.CallMethod("getCurrentData", params);

				//iterate on result assuming it is an array
				for (Json::Value::ArrayIndex i = 0; i < result.size(); i++) {
					//each element is a json object naming a modbus block
					Json::Value block = result[i];
					//iterate for key value pairs on block
					for (auto const& key : block.getMemberNames()) {
						//Check if key contains "Interfaces::pvinverter::"
						if(key.find("Interfaces::pvinverter::") != std::string::npos && !inverterThings.isEmpty()){
							//Extract stateName from key after "Interfaces::pvinverter::"
							QString stateName = QString::fromStdString(key.substr(24));
							//Get stateValue for variant type
							QVariant stateValue = inverterThings.first()->stateValue(stateName);
							//Get type from variant
							if(inverterThings.first()->hasState(stateName) && stateValue.type() == QVariant::Double){
								inverterThings.first()->setStateValue(stateName, block[key].asFloat());
								// qCDebug(dcGenericConsolinno()) << "stateValue:" << block[key].asFloat();
							}
							inverterDataFound = true;
						} else if(key.find("Interfaces::meter::") != std::string::npos && !meterThings.isEmpty()){
							//Extract stateName from key after "Interfaces::meter::"
							QString stateName = QString::fromStdString(key.substr(19));
							//Get stateValue for variant type
							QVariant stateValue = meterThings.first()->stateValue(stateName);
							//Get type from variant
							if(meterThings.first()->hasState(stateName) && stateValue.type() == QVariant::Double){
								meterThings.first()->setStateValue(stateName, block[key].asFloat());
							}
							meterDataFound = true;
						} else if(key.find("Interfaces::battery::") != std::string::npos && !batteryThings.isEmpty()){
							//Extract stateName from key after "Interfaces::battery::"
							QString stateName = QString::fromStdString(key.substr(21));
							//Get stateValue for variant type
							QVariant stateValue = batteryThings.first()->stateValue(stateName);
							//qCDebug(dcGenericConsolinno()) << "bat name" << stateName << "stateValue:" << stateValue;
							//Get type from variant
							if(batteryThings.first()->hasState(stateName)){
								batteryThings.first()->setStateValue(stateName, block[key].asFloat());
								//qCDebug(dcGenericConsolinno()) << "bat name" << stateName << "block[key].asFloat():" << block[key].asFloat();

								// start battery timer if remote control is active and battery timer is not started yet
								if (stateName == "enableForcePowerState" && m_batteryPowerTimer) {
									bool enableToggle = batteryThings.first()->stateValue(batteryEnableForcePowerStateTypeId).toBool();
									int timeout = batteryThings.first()->stateValue(batteryForcePowerTimeoutStateTypeId).toInt();
									if (enableToggle && stateValue.toBool()) {
										if (!m_batteryPowerTimer->isActive()) {
											m_batteryPowerTimer->start((timeout/2)*1000);
										}
									}
								}
							}
							batteryDataFound = true;
						}
					}
				}

				thing->setStateValue(genericConsolinnoConnectionConnectedStateTypeId, inverterDataFound);
				

			} catch (JsonRpcException &e) {
				// std::cerr << e.what() << std::endl;
				thing->setStateValue(genericConsolinnoConnectionConnectedStateTypeId, false);
			}

			if(!inverterThings.isEmpty()){
				inverterThings.first()->setStateValue(inverterConnectedStateTypeId, inverterDataFound);
			}
			if(!meterThings.isEmpty()){
				meterThings.first()->setStateValue(meterConnectedStateTypeId, meterDataFound);
			}
			if(!batteryThings.isEmpty()){
				batteryThings.first()->setStateValue(batteryConnectedStateTypeId, batteryDataFound);
			}


		});
		info->finish(Thing::ThingErrorNoError);    
        
    } else if (thing->thingClassId() == inverterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(inverterConnectedStateTypeId, parentThing->stateValue(genericConsolinnoConnectionConnectedStateTypeId).toBool());
        }		
    } else if (thing->thingClassId() == meterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(meterConnectedStateTypeId, parentThing->stateValue(genericConsolinnoConnectionConnectedStateTypeId).toBool());
        }		
    } else if (thing->thingClassId() == batteryThingClassId) {

        // Set battery capacity from settings on restart.
        thing->setStateValue(batteryCapacityStateTypeId, thing->setting(batterySettingsCapacityParamTypeId).toUInt());


        // Set battery capacity on settings change.
        connect(thing, &Thing::settingChanged, this, [this, thing] (const ParamTypeId &paramTypeId, const QVariant &value) {
            if (paramTypeId == batterySettingsCapacityParamTypeId) {
                qCDebug(dcGenericConsolinno()) << "Battery capacity changed to" << value.toInt() << "kWh";
                thing->setStateValue(batteryCapacityStateTypeId, value.toInt());
            }
        });

		// battery power timer
		m_batteryPowerTimer = new QTimer(this);

		// battery power set
		connect(m_batteryPowerTimer, &QTimer::timeout, thing, [this, thing]() {
			bool enable = thing->stateValue(batteryEnableForcePowerStateTypeId).toBool();

			std::cout << "battery timer triggered" << std::endl;
			if (enable) {
				setBatteryPower(thing);
			} else {
				disableRemoteControl(thing);
			}
			
		});

		int timeout = thing->stateValue(batteryForcePowerTimeoutStateTypeId).toInt();
		// send command not every timeout seconds, but half of it (FoxESS sometimes counts significantly faster than reality, 60s in reality is ~30s for FoxESS)
		m_batteryPowerTimer->start(timeout/2*1000);

        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(batteryConnectedStateTypeId, parentThing->stateValue(genericConsolinnoConnectionConnectedStateTypeId).toBool());
        }

		info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginGenericConsolinno::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == genericConsolinnoConnectionThingClassId) {
        // Check if w have to set up a child meter/inverter/battery for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(meterThingClassId).isEmpty()) {
            qCDebug(dcGenericConsolinno()) << "Setup new meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(meterThingClassId, thing->name() + " meter", QString(), thing->id()));
        }
		if (myThings().filterByParentId(thing->id()).filterByThingClassId(inverterThingClassId).isEmpty()) {
            qCDebug(dcGenericConsolinno()) << "Setup new inverter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(inverterThingClassId, thing->name() + " inverter", QString(), thing->id()));
        }
		if (myThings().filterByParentId(thing->id()).filterByThingClassId(batteryThingClassId).isEmpty()) {
            qCDebug(dcGenericConsolinno()) << "Setup new battery for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(batteryThingClassId, thing->name() + " battery", QString(), thing->id()));
        }
    }
}

void IntegrationPluginGenericConsolinno::executeAction(ThingActionInfo *info)
{
	Json::Value result;
	std::string remoteFunctionName = "none";

	if (info->action().actionTypeId() == inverterSetExportLimitActionTypeId) {
		// params
		remoteFunctionName = "setExportLimit";
		int valuePercent = info->action().paramValue(inverterSetExportLimitActionExportLimitParamTypeId).toInt();
		int nominalPowerWatt = info->action().paramValue(inverterSetExportLimitActionNominalPowerParamTypeId).toInt();

		// std::cout << "action, val: " << valuePercent << std::endl;

		Thing *parentThing = myThings().findById(info->thing()->parentId());
		QString connectionParams = parentThing->paramValue(genericConsolinnoConnectionThingConnectionParamsParamTypeId).toString();
		//Jsonrpc request to set limit
		HttpClient client(urlModbusRTU);
		Client c(client);
		Json::Value params;
		params["connectionParams"] = connectionParams.toStdString();
		params["value"] = valuePercent;
		params["nominalPower"] = nominalPowerWatt;

		QString errMes = "exception";
		try {
			result = c.CallMethod(remoteFunctionName, params);
		} catch (JsonRpcException &e) {
			// std::cerr << e.what() << std::endl;
			// info->thing()->setStateValue(genericConsolinnoConnectionConnectedStateTypeId, false);
			errMes = QString::fromStdString("export limit failed: " + std::string(e.what()));
			info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP(errMes));
		}

	} else if (info->action().actionTypeId() == batteryEnableForcePowerActionTypeId) {
		
		bool enableToggle = info->action().paramValue(batteryEnableForcePowerActionEnableForcePowerParamTypeId).toBool();
		int batteryTimeout = info->thing()->stateValue(batteryForcePowerTimeoutStateTypeId).toInt();

		std::cout << "mode changed:" << enableToggle << std::endl;

		if (enableToggle) {
			// timer is not restart on every toggle change, but sent continuosly, start counter if not started yet
			if (!m_batteryPowerTimer->isActive()) {
				m_batteryPowerTimer->start((batteryTimeout/2)*1000);
			}
			//but trigger sent once:
			remoteFunctionName = "setBatteryPower";
			result = setBatteryPower(info->thing());
		} else {
			//disable forcecharge once
			remoteFunctionName = "disableRemoteControl";
			result = disableRemoteControl(info->thing());
		}

		info->thing()->setStateValue(batteryEnableForcePowerStateTypeId, enableToggle);

	} else if (info->action().actionTypeId() == batteryForcePowerTimeoutActionTypeId) {
		int batteryTimeout = info->action().paramValue(batteryForcePowerTimeoutActionForcePowerTimeoutParamTypeId).toInt();
		bool enableToggle = info->thing()->stateValue(batteryEnableForcePowerStateTypeId).toBool();

		if (enableToggle) {
			remoteFunctionName = "setBatteryPower";
			result = setBatteryPower(info->thing(), UNSET_INT, batteryTimeout);
			//restart timer with new timeout
			if (m_batteryPowerTimer->isActive()) {
				m_batteryPowerTimer->stop();
				m_batteryPowerTimer->start((batteryTimeout/2)*1000);
			} else {
				m_batteryPowerTimer->start((batteryTimeout/2)*1000);
			}
		}

		info->thing()->setStateValue(batteryForcePowerTimeoutStateTypeId, batteryTimeout);

	} else if (info->action().actionTypeId() == batteryForcePowerActionTypeId) {
		int batteryPower = info->action().paramValue(batteryForcePowerActionForcePowerParamTypeId).toInt();
		bool enableToggle = info->thing()->stateValue(batteryEnableForcePowerStateTypeId).toBool();

		if (enableToggle) {
			remoteFunctionName = "setBatteryPower";
			result = setBatteryPower(info->thing(), batteryPower);
		}

		info->thing()->setStateValue(batteryForcePowerStateTypeId, batteryPower);

	}
    
	if (result.get("state", "failed").asString() == "successful") {
		info->finish(Thing::ThingErrorNoError);
	} else {
		QString errMes = "none";
		if (remoteFunctionName == "setBatteryPower") {
			errMes = QString::fromStdString(remoteFunctionName + " failed.");
		} else {
			errMes = QString::fromStdString(remoteFunctionName + " failed: " + result.get("reason","unknown").asString());
		}
		
		info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP(errMes));
	}
}

void IntegrationPluginGenericConsolinno::thingRemoved(Thing *thing)
{
	// A thing is being removed from the system. Do the required cleanup
    // (e.g. disconnect from the device) here.

    qCDebug(dcGenericConsolinno()) << "thingRemoved : Remove thing" << thing;

	if (m_timer && myThings().isEmpty()) {
        qCDebug(dcGenericConsolinno()) << "Stopping refresh timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_timer);
        m_timer = nullptr;
    }
	
	if (m_batteryPowerTimer && m_batteryPowerTimer->isActive()) {
		m_batteryPowerTimer->stop();
		m_batteryPowerTimer = nullptr;
	}

}


Json::Value IntegrationPluginGenericConsolinno::setBatteryPower(Thing* thing, int power, int timeout) {	
	Json::Value result;
	if (thing->thingClassId() == batteryThingClassId) {
		if (power == UNSET_INT) {
			power = thing->stateValue(batteryForcePowerStateTypeId).toInt();
		}
		if (timeout == UNSET_INT) {
			timeout = thing->stateValue(batteryForcePowerTimeoutStateTypeId).toInt();
		}
		int nominalPower = thing->stateValue(batteryNominalPowerBatteryStateTypeId).toInt();
		

		Thing *parentThing = myThings().findById(thing->parentId());
		QString connectionParams = parentThing->paramValue(genericConsolinnoConnectionThingConnectionParamsParamTypeId).toString();
		//Jsonrpc request to set limit
		HttpClient client(urlModbusRTU);
		Client c(client);
		Json::Value params;
		params["connectionParams"] = connectionParams.toStdString();
		params["power"] = power;
		params["timeout"] = timeout;
		params["nominalPower"] = nominalPower;
		
		//QString errMes = "exception";
		try {
			std::cout << "setBatteryPower sent, power: " << power << ", timeout: " << timeout << std::endl;
			result["reason"] = c.CallMethod("setBatteryPower", params);
			result["state"] = "successful";
		} catch (JsonRpcException &e) {
			//std::cerr << e.what() << std::endl;
			//errMes = QString::fromStdString("setBatteryPower failed: " + std::string(e.what()));
			result["state"] = "failed";
			result["reason"] = "setBatteryPower failed";// + std::string(e.what());
		}
	} else {
		result["state"] = "failed";
		result["reason"] = "thing does not support setBatteryPower";
	}
	return result;
}

Json::Value IntegrationPluginGenericConsolinno::disableRemoteControl(Thing *thing) {
	Json::Value result;
	if (thing->thingClassId() == batteryThingClassId) {

		Thing *parentThing = myThings().findById(thing->parentId());
		QString connectionParams = parentThing->paramValue(genericConsolinnoConnectionThingConnectionParamsParamTypeId).toString();
		//Jsonrpc request to set limit
		HttpClient client(urlModbusRTU);
		Client c(client);
		Json::Value params;
		params["connectionParams"] = connectionParams.toStdString();
		
		//std::string errMes = "exception";
		try {
			std::cout << "disableRemoteControl sent" << std::endl;
			result["reason"] = c.CallMethod("disableRemoteControl", params);
			result["state"] = "successful";
		} catch (JsonRpcException &e) {
			// std::cerr << e.what() << std::endl;
			//errMes = "disableRemoteControl failed: " + std::string(e.what());
			result["state"] = "failed";
			result["reason"] = "disableRemoteControl failed: " + std::string(e.what());
		}
	} else {
		result["state"] = "failed";
		result["reason"] = "thing does not support setBatteryPower";
	}
	return result;
}
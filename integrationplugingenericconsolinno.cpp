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

#include "network/networkdevicediscovery.h"
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

	HttpClient client(urlModbusRTU);
	Client c(client);

	Json::Value params;
	params["identifier"] = "";

	try {
		//Save c.CallMethod("discoverAllDevices", params); responce to json object
		Json::Value result = c.CallMethod("discoverAllDevices", params);
		//iterate on result
		for (Json::Value::ArrayIndex i = 0; i < result.size(); i++) {
			//Debug out result[i]
			std::cout << "result[" << i << "]:" << result[i] << std::endl;
			//Call processDiscoveryObject with result[i] and info
			processDiscoveryObject(result[i], info);
		}
		info->finish(Thing::ThingErrorNoError);
	} catch (JsonRpcException &e) {
		std::cerr << e.what() << std::endl;
		info->finish(Thing::ThingErrorHardwareFailure);
	}
	
}

void IntegrationPluginGenericConsolinno::processDiscoveryObject(const Json::Value &responseObject, ThingDiscoveryInfo *info){
	ThingDescriptor descriptor(inverterThingClassId, "Generic Consolinno Inverter", "Generic Consolinno Inverter");
	ParamList params;
	params << Param(inverterThingConnectionParamsParamTypeId, QVariant::fromValue((QString::fromStdString(responseObject.asString()))));
	descriptor.setParams(params);
	info->addThingDescriptor(descriptor);
}

void IntegrationPluginGenericConsolinno::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcGenericConsolinno()) << "Setup" << thing << thing->params();
	m_timer = hardwareManager()->pluginTimerManager()->registerTimer(1);

    if (thing->thingClassId() == inverterThingClassId) {		

		connect(m_timer, &PluginTimer::timeout, thing, [this, thing](){
			QString connectionParams = thing->paramValue(inverterThingConnectionParamsParamTypeId).toString();
			// qCDebug(dcGenericConsolinno()) << "connectionParams:" << connectionParams;
			//Jsonrpc request to get currentData
			HttpClient client(urlModbusRTU);
			Client c(client);
			Json::Value params;
			params["connectionParams"] = connectionParams.toStdString();
			params["block"] = -1;
			try {
				//Save c.CallMethod("discoverAllDevices", params); responce to json object
				Json::Value result = c.CallMethod("getCurrentData", params);
				//iterate on result assuming it is an array
				for (Json::Value::ArrayIndex i = 0; i < result.size(); i++) {
					//each element is a json object naming a modbus block
					Json::Value block = result[i];
					//iterate for key value pairs on block
					for (auto const& key : block.getMemberNames()) {
						//Debug out key and value
						// std::cout << "key:" << key << " value:" << block[key] << std::endl;
						if(key == "Interfaces::pvinverter::frequency"){
							thing->setStateValue(inverterFrequencyStateTypeId, block[key].asFloat());
						} else if(key == "Interfaces::pvinverter::totalEnergyProduced"){
							thing->setStateValue(inverterTotalEnergyProducedStateTypeId, block[key].asFloat());
						} else if(key == "Interfaces::pvinverter::pvVoltage1"){
							thing->setStateValue(inverterPvVoltage1StateTypeId, block[key].asFloat());
						} else if(key == "Interfaces::pvinverter::pvVoltage2"){//Interfaces::pvinverter::pvVoltage2
							thing->setStateValue(inverterPvVoltage2StateTypeId, block[key].asFloat());
						} else if(key == "Interfaces::pvinverter::voltagePhaseA"){//Interfaces::pvinverter::voltagePhaseA
							thing->setStateValue(inverterVoltagePhaseAStateTypeId, block[key].asFloat());
						} else if(key == "Interfaces::pvinverter::voltagePhaseB"){//Interfaces::pvinverter::voltagePhaseB
							thing->setStateValue(inverterVoltagePhaseBStateTypeId, block[key].asFloat());
						} else if(key == "Interfaces::pvinverter::voltagePhaseC"){//Interfaces::pvinverter::voltagePhaseC
							thing->setStateValue(inverterVoltagePhaseCStateTypeId, block[key].asFloat());
						}
					}				
				}

				thing->setStateValue(inverterConnectedStateTypeId, true);				
			} catch (JsonRpcException &e) {
				// std::cerr << e.what() << std::endl;
				thing->setStateValue(inverterConnectedStateTypeId, false);
			}
		});
		info->finish(Thing::ThingErrorNoError);    
        
    } else if (thing->thingClassId() == meterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(meterConnectedStateTypeId, parentThing->stateValue(inverterConnectedStateTypeId).toBool());
        }
    } else if (thing->thingClassId() == batteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);

        // Set battery capacity from settings on restart.
        thing->setStateValue(batteryCapacityStateTypeId, thing->setting(batterySettingsCapacityParamTypeId).toUInt());

        // Set battery capacity on settings change.
        connect(thing, &Thing::settingChanged, this, [this, thing] (const ParamTypeId &paramTypeId, const QVariant &value) {
            if (paramTypeId == batterySettingsCapacityParamTypeId) {
                qCDebug(dcGenericConsolinno()) << "Battery capacity changed to" << value.toInt() << "kWh";
                thing->setStateValue(batteryCapacityStateTypeId, value.toInt());
            }
        });

        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(batteryConnectedStateTypeId, parentThing->stateValue(inverterConnectedStateTypeId).toBool());
        }
    }
}

void IntegrationPluginGenericConsolinno::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == inverterThingClassId) {
        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(meterThingClassId).isEmpty()) {
            qCDebug(dcGenericConsolinno()) << "Setup new meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(meterThingClassId, "Generic Consolinno Energy Meter", QString(), thing->id()));
        }
    }
}

void IntegrationPluginGenericConsolinno::executeAction(ThingActionInfo *info)
{
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginGenericConsolinno::thingRemoved(Thing *thing)
{
	// A thing is being removed from the system. Do the required cleanup
    // (e.g. disconnect from the device) here.

    qCDebug(dcGenericConsolinno()) << "thingRemoved : Remove thing" << thing;
}


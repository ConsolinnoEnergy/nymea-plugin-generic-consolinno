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

#ifndef INTEGRATIONPLUGINGENERICCONSOLINNO_H
#define INTEGRATIONPLUGINGENERICCONSOLINNO_H

#include <QObject>
#include <QTimer>
#include <integrations/integrationplugin.h>
#include <QString>
class PluginTimer;
#include <QNetworkRequest>
#include <QtNetwork/QTcpSocket>
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsoncpp/json/json.h>

using namespace jsonrpc;
using namespace std;

class IntegrationPluginGenericConsolinno : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationplugingenericconsolinno.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginGenericConsolinno();

    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
	void sendDiscoveryRequest();
	void readDiscoveryResponse(ThingDiscoveryInfo *info);
	void handleDiscoveryResponse(const QByteArray &responseData, ThingDiscoveryInfo *info);
	void processDiscoveryObject(const Json::Value &responseObject, ThingDiscoveryInfo *info);
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;

private:
    PluginTimer *m_timer = nullptr;
    // PluginTimer *m_timer2 = nullptr;  // apparently the plugin does not work with 2 PluginTimers
    QTcpSocket *m_sockets;

	std::string urlModbusRTU= "http://127.0.0.1:8383";

};

#endif // INTEGRATIONPLUGINGENERICCONSOLINNO_H

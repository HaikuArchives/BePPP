#include <net_settings.h>
#include <Application.h>
#include <Window.h>
#include <Screen.h>
#include <CheckBox.h>
#include <String.h>
#include <Alert.h>
#include <Entry.h>
#include <Roster.h>
#include <Path.h>

class ConfigWindow : public BWindow {
	public:
		ConfigWindow(void) : BWindow(Center(120,70),"PPPoE Config",B_TITLED_WINDOW,B_NOT_RESIZABLE | B_NOT_ZOOMABLE) {
			BRect box = Bounds();
			
			box.bottom /= 2;
			box.bottom -= 5;
			
			BView *gray = new BView(Bounds(),"gray",B_FOLLOW_ALL_SIDES,B_WILL_DRAW);
			gray->SetViewColor(216,216,216);
			on = new BCheckBox(box,"pppoe_on","PPPoE Active",new BMessage('ppoe'));
			char setting[255];
			find_net_setting(NULL,"GLOBAL","PROTOCOLS",setting,255);
			if (BString(setting).FindFirst("pppoe") >= 0)
				on->SetValue(B_CONTROL_ON);
			else
				on->SetValue(B_CONTROL_OFF);
			gray->AddChild(on);
			
			box = Bounds();
			
			box.top = (box.bottom / 2) + 5;
			
			ipcp = new BCheckBox(box,"drop_ipcp","Force Manual DNS",new BMessage('ipcp'));
			find_net_setting(NULL,"pppoe","drop_second_ipcp",setting,255);
			if (BString(setting).IFindFirst("true") >= 0)
				ipcp->SetValue(B_CONTROL_ON);
			else
				ipcp->SetValue(B_CONTROL_OFF);
			gray->AddChild(ipcp);
			AddChild(gray);
		}
		void MessageReceived(BMessage *msg) {
			if (msg->what == 'ppoe') {
				BString value;
				char setting[255];
				find_net_setting(NULL,"GLOBAL","PROTOCOLS",setting,255);
				value = setting;
				
				value.RemoveAll("pppoe ");
				value.RemoveAll(" pppoe");
				value.RemoveAll("pppoe");
				if (on->Value() == B_CONTROL_ON)
					value += " pppoe";
				set_net_setting(NULL,"GLOBAL","PROTOCOLS",value.String());
			} else if (msg->what == 'ipcp') {
				set_net_setting(NULL,"pppoe",NULL,NULL);
				set_net_setting(NULL,"pppoe","drop_second_ipcp",(ipcp->Value() == B_CONTROL_ON) ? "true" : "false");
			}
			BWindow::MessageReceived(msg);
		}
		bool QuitRequested(void) {
			be_app->Quit();
			return true;
		}
	private:
		BCheckBox *on, *ipcp;
		BRect Center(float width,float height) {
			BRect screen = BScreen().Frame();
			return BRect((screen.right/2) - (width/2),(screen.bottom/2) - (height/2),(screen.right/2) + (width/2),(screen.bottom/2) + (height/2));
		}
};

int main (void) {
	new BApplication("application/x-vnd.nwhitehorn-pppoe");
	app_info info;
	be_app->GetAppInfo(&info);
	BPath path(&(info.ref));
	if (!((strcmp(path.Path(),"/boot/beos/system/add-ons/net_server/pppoe") == 0) || ((strcmp(path.Path(),"/boot/home/config/add-ons/net_server/pppoe") == 0)))) {
		BAlert *alert = new BAlert("oops","I don't seem to be located in the net_server add-ons directory. Would you like me to move there?","Cancel","OK");
		if (alert->Go() == 1)
			BEntry(path.Path()).Rename("/boot/home/config/add-ons/net_server/pppoe",true);
	}
	ConfigWindow *window = new ConfigWindow();
	window->Show();
	be_app->Run();
	return 0;
}
	
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
		ConfigWindow(void) : BWindow(Center(100,30),"CHAP Config",B_TITLED_WINDOW,B_NOT_RESIZABLE | B_NOT_ZOOMABLE) {
			BView *gray = new BView(Bounds(),"gray",B_FOLLOW_ALL_SIDES,B_WILL_DRAW);
			gray->SetViewColor(216,216,216);
			on = new BCheckBox(Bounds(),"chap_on","CHAP Active",new BMessage('ppoe'));
			char setting[255];
			find_net_setting(NULL,"GLOBAL","PROTOCOLS",setting,255);
			if (BString(setting).FindFirst("chap") >= 0)
				on->SetValue(B_CONTROL_ON);
			else
				on->SetValue(B_CONTROL_OFF);
			gray->AddChild(on);
			AddChild(gray);
		}
		void MessageReceived(BMessage *msg) {
			if (msg->what = 'ppoe') {
				BString value;
				char setting[255];
				find_net_setting(NULL,"GLOBAL","PROTOCOLS",setting,255);
				value = setting;
				
				value.RemoveAll("chap ");
				value.RemoveAll(" chap");
				value.RemoveAll("chap");
				if (on->Value() == B_CONTROL_ON)
					value += " chap";
				set_net_setting(NULL,"GLOBAL","PROTOCOLS",value.String());
			}
			BWindow::MessageReceived(msg);
		}
		bool QuitRequested(void) {
			be_app->Quit();
			return true;
		}
	private:
		BCheckBox *on;
		BRect Center(float width,float height) {
			BRect screen = BScreen().Frame();
			return BRect((screen.right/2) - (width/2),(screen.bottom/2) - (height/2),(screen.right/2) + (width/2),(screen.bottom/2) + (height/2));
		}
};

int main (void) {
	new BApplication("application/x-vnd.nwhitehorn-chap");
	app_info info;
	be_app->GetAppInfo(&info);
	BPath path(&(info.ref));
	if (!((strcmp(path.Path(),"/boot/beos/system/add-ons/net_server/chap") == 0) || ((strcmp(path.Path(),"/boot/home/config/add-ons/net_server/chap") == 0)))) {
		BAlert *alert = new BAlert("oops","I don't seem to be located in the net_server add-ons directory. Would you like me to move there?","Cancel","OK");
		if (alert->Go() == 1)
			BEntry(path.Path()).Rename("/boot/home/config/add-ons/net_server/chap",true);
	}
	ConfigWindow *window = new ConfigWindow();
	window->Show();
	be_app->Run();
	return 0;
}
	
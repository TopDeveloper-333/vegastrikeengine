/* 
 * Vega Strike
 * Copyright (C) 2001-2002 Daniel Horn
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#if defined(HAVE_SDL)
#include <SDL/SDL.h>
#endif

#if defined(WITH_MACOSX_BUNDLE)
#import <sys/param.h>
#endif
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#endif
#include "gfxlib.h"
#include "in_kb.h"
#include "lin_time.h"
#include "main_loop.h"
#include "config_xml.h"
#include "cmd/script/mission.h"
#include "audiolib.h"
#include "vs_path.h"
#include "vs_globals.h"
#include "gfx/animation.h"
#include "cmd/unit.h"
#include "gfx/cockpit.h"
#include "python/init.h"
#include "savegame.h"
#include "force_feedback.h"
#include "gfx/hud.h"
#include "gldrv/winsys.h"
#include "universe_util.h"
#include "networking/netclient.h"

/*
 * Globals 
 */
ForceFeedback *forcefeedback;

TextPlane *bs_tp=NULL;

/* 
 * Function definitions
 */

void setup_game_data ( ){ //pass in config file l8r??
  g_game.audio_frequency_mode=4;//22050/16
  g_game.sound_enabled =1;
  g_game.use_textures =1;
  g_game.use_ship_textures =0;
  g_game.use_planet_textures =0;
  g_game.use_sprites =1;
  g_game.use_animations =1;
  g_game.use_logos =1;
  g_game.music_enabled=1;
  g_game.sound_volume=1;
  g_game.music_volume=1;
  g_game.warning_level=20;
  g_game.capture_mouse=GFXFALSE;
  g_game.y_resolution = 768;
  g_game.x_resolution = 1024;
  g_game.fov=78;
  g_game.MouseSensitivityX=2;
  g_game.MouseSensitivityY=4;

}
bool STATIC_VARS_DESTROYED=false;
void ParseCommandLine(int argc, char ** CmdLine);
void cleanup(void)
{
	if( Network!=NULL)
	{
		for( int i=0; i<_Universe->numPlayers(); i++)
			Network[i].logout();
		delete [] Network;
	}

  STATIC_VARS_DESTROYED=true;
  printf ("Thank you for playing!\n");
  _Universe->WriteSaveGame(true);
  winsys_shutdown();
  //    write_config_file();
  AUDDestroy();
  //destroyObjects();
  //Unit::ProcessDeleteQueue();
  delete _Universe;
    delete [] CONFIGFILE;

  
}

VegaConfig *vs_config;
Mission *mission;
LeakVector<Mission *> active_missions;
double benchmark=-1.0;

char mission_name[1024];

void bootstrap_main_loop();

#if defined(WITH_MACOSX_BUNDLE)
 #undef main
#endif

int main( int argc, char *argv[] ) 
{
	CONFIGFILE=0;
	mission_name[0]='\0';
//	char *TEST_STUFF=NULL;
//	*TEST_STUFF='0';//MAKE IT CRASH
#if defined(WITH_MACOSX_BUNDLE)||defined(_WIN32)
    // We need to set the path back 2 to make everything ok.
	{
    char *parentdir;
	int pathlen=strlen(argv[0]);
	parentdir=new char[pathlen+1];
    char *c;
    strncpy ( parentdir, argv[0], pathlen+1 );
    c = (char*) parentdir;

    while (*c != '\0')     /* go to end */
        c++;
    
    while ((*c != '/')&&(*c != '\\')&&c>parentdir)      /* back up to parent */
        c--;
    
    *c++ = '\0';             /* cut off last part (binary name) */
    if (strlen (parentdir)>0) {  
      chdir (parentdir);/* chdir to the binary app's parent */
    }
	delete []parentdir;
	}
#if defined(WITH_MACOSX_BUNDLE)
    chdir ("../../../");/* chdir to the .app's parent */
#endif
#endif
    /* Print copyright notice */
  fprintf( stderr, "Vega Strike "  " \n"
	     "See http://www.gnu.org/copyleft/gpl.html for license details.\n\n" );
    /* Seed the random number generator */
    

    if(benchmark<0.0){
      srand( time(NULL) );
    }
    else{
      // in benchmark mode, always use the same seed
      srand(171070);
    }
    //this sets up the vegastrike config variable
    setup_game_data(); 
    // loads the configuration file .vegastrikerc from home dir if such exists
    ParseCommandLine(argc,argv);
	if (CONFIGFILE==0) {
		CONFIGFILE=new char[42];
		sprintf(CONFIGFILE,"vegastrike.config");
	}
		
    initpaths();
    //can use the vegastrike config variable to read in the default mission

  g_game.music_enabled = XMLSupport::parse_bool (vs_config->getVariable ("audio","Music","true"));
#ifndef _WIN32
  if (g_game.music_enabled) {
    int pid=fork();
    if (!pid) {
      pid=execlp("./soundserver","./soundserver",NULL);
      g_game.music_enabled=false;
      fprintf(stderr,"Unable to spawn music player server\n");
      exit (0);
    } else {
      if (pid==-1) {
	g_game.music_enabled=false;
      }
    }
  }
#endif
    if (mission_name[0]=='\0')
      strcpy(mission_name,vs_config->getVariable ("general","default_mission","test1.mission").c_str());
    //might overwrite the default mission with the command line

#ifdef HAVE_PYTHON
    Python::init();
    Python::initpaths();
    Python::test();
#endif

    

#if defined(HAVE_SDL)
    // && defined(HAVE_SDL_MIXER)
  if (  SDL_InitSubSystem( SDL_INIT_JOYSTICK )) {
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        winsys_exit(1);
    }

#endif
#if 0
    InitTime();
    UpdateTime();
#endif

    AUDInit();
    AUDListenerGain (XMLSupport::parse_float(vs_config->getVariable ("audio","sound_gain",".5")));   
    /* Set up a function to clean up when program exits */
    winsys_atexit( cleanup );
    /*
#if defined(HAVE_SDL) && defined(HAVE_SDL_MIXER)

    init_audio_data();
    init_audio();
    init_joystick();

#endif
    */
    _Universe= new Universe(argc,argv,vs_config->getVariable ("general","galaxy","milky_way.xml").c_str());   
    _Universe->Loop(bootstrap_main_loop);
    return 0;
}
  static Animation * SplashScreen = NULL;
static bool BootstrapMyStarSystemLoading=true;
void SetStarSystemLoading (bool value) {
  BootstrapMyStarSystemLoading=value;
}

void bootstrap_draw (const std::string &message, float x, float y, Animation * newSplashScreen) {

  static Animation *ani=NULL;
  if (!BootstrapMyStarSystemLoading) {
    return;
  }
  if(SplashScreen==NULL){
    // if there's no splashscreen, we don't draw on it
    // this happens, when the splash screens texture is loaded
    return;
  }

  if (newSplashScreen!=NULL) {
    ani = newSplashScreen;
  }
  UpdateTime();

  Matrix tmp;
  Identity (tmp);
  BootstrapMyStarSystemLoading=false;
  static Texture dummy ("white.bmp");
  BootstrapMyStarSystemLoading=true;
  dummy.MakeActive();
  GFXDisable(LIGHTING);
  GFXDisable(DEPTHTEST);
  GFXBlendMode (ONE,ZERO);
  GFXEnable (TEXTURE0);
  GFXDisable (TEXTURE1);
  GFXColor4f (1,1,1,1);
  GFXClear (GFXTRUE);
  GFXLoadIdentity (PROJECTION);
  GFXLoadIdentity(VIEW);
  GFXLoadMatrixModel (tmp);
  GFXBeginScene();

  bs_tp->SetPos (x,y);
  bs_tp->SetCharSize (.4,.8);
  ScaleMatrix (tmp,Vector (7,7,0));
  GFXLoadMatrixModel (tmp);
  GFXHudMode (GFXTRUE);
  if (ani) {
    ani->UpdateAllFrame();
    ani->DrawNow(tmp);
  }
  bs_tp->Draw (message);  
  GFXHudMode (GFXFALSE);
  GFXEndScene();

}
extern Unit **fighters;


bool SetPlayerLoc (QVector &sys, bool set) {
  static QVector mysys;
  static bool isset=false;
  if (set) {
    isset = true;
    mysys = sys;
    return true;
  }else {
    if (isset)
      sys =mysys;
    return isset;
  }  
  
}
bool SetPlayerSystem (std::string &sys, bool set) {
  static std::string mysys;
  static bool isset=false;
  if (set) {
    isset = true;
    mysys = sys;
    return true;
  }else {
    if (isset)
      sys =mysys;
    return isset;
  }
}

void bootstrap_main_loop () {

  static bool LoadMission=true;
  InitTime();

  //  bootstrap_draw ("Beginning Load...",SplashScreen);
  //  bootstrap_draw ("Beginning Load...",SplashScreen);
  if (LoadMission) {
    LoadMission=false;
    active_missions.push_back(mission=new Mission(mission_name));

    mission->initMission();

    bs_tp=new TextPlane();
 
    SplashScreen = new Animation (mission->getVariable ("splashscreen",vs_config->getVariable ("graphics","splash_screen","vega_splash.ani")).c_str(),0);
    bootstrap_draw ("Vegastrike Loading...",-.135,0,SplashScreen);
  if (g_game.music_enabled) {
#ifdef _WIN32
//    int pid=spawnl(P_NOWAIT,"./soundserver","./soundserver",NULL);
	  int pid=(int)ShellExecute(NULL,"open","./soundserver.exe","","./",SW_MINIMIZE);
    if (!pid) {
      g_game.music_enabled=false;
      fprintf(stderr,"Unable to spawn music player server\n");
    }
#endif
  }
    QVector pos;
    string planetname;

    mission->GetOrigin(pos,planetname);
    bool setplayerloc=false;
    string mysystem = mission->getVariable("system","sol.system");
    int numplayers = XMLSupport::parse_int (mission->getVariable ("num_players","1"));
    vector <std::string>playername;
    vector <std::string>playerpasswd;
    for (int p=0;p<numplayers;p++) {
      playername.push_back(vs_config->getVariable("player"+((p>0)?tostring(p+1):string("")),"callsign",""));
	  playerpasswd.push_back(vs_config->getVariable("player"+((p>0)?tostring(p+1):string("")),"password",""));
    }

	/************************* NETWORK INIT ***************************/
	cout<<"Number of local players = "<<numplayers<<endl;
	// Initiate the network if in networking play mode for each local player
	string srvip = vs_config->getVariable("network","server_ip","");
	if( srvip != "")
	{
		string srvport = vs_config->getVariable("network","server_port",SERVER_PORT);
		// Get the number of local players
		Network = new NetClient[numplayers];

		cout<<endl<<"Server IP : "<<srvip<<" - port : "<<srvport<<endl<<endl;
		char * srvipadr = new char[srvip.length()+1];
		memcpy( srvipadr, srvip.c_str(), srvip.length());
		srvipadr[srvip.length()] = '\0';
		int port = atoi( srvport.c_str());
		cout<<"Port : "<<port<<endl;
		vector<std::string>::iterator i, j;
		int nbii = 0;
		for( i=playername.begin(), j=playerpasswd.begin(); i!=playername.end(); i++, j++, nbii++)
		{
			int nbok = 0;
			if( Network[nbii].init( srvipadr, (unsigned short) port) == -1)
			{
				// If network initialization fails, exit
				cleanup();
				cout<<"Network initialization error - exiting"<<endl;
				exit( 1);
			}
			//sleep( 3);
			cout<<"Waiting for player "<<(nbii+1)<<" = "<<(*i)<<":"<<(*j)<<"login response...";
			nbok = Network[nbii].loginLoop( (*i), (*j));
			if( !nbok)
			{
				cout<<"No server response, exiting"<<endl;
				exit(1);
			}
			else
				cout<<" logged in !"<<endl;
		}
	}
	else
	{
		Network = NULL;
		cout<<"Non-networking mode"<<endl;
	}
	/************************* NETWORK INIT ***************************/

    _Universe->SetupCockpits(playername);
    float credits=XMLSupport::parse_float (mission->getVariable("credits","0"));
    g_game.difficulty=XMLSupport::parse_float (mission->getVariable("difficulty","1"));
    string savegamefile = mission->getVariable ("savegame","");
    vector <SavedUnits> savedun;
    vector <string> playersaveunit;
	vector <StarSystem *> ss;
	vector <string> starsysname;
	vector <QVector> playerNloc;
    for (unsigned int k=0;k<_Universe->cockpit.size();k++) {
      bool setplayerXloc=false;
      std::string psu;
      if (k==0) {
		QVector myVec;
		if (SetPlayerLoc (myVec,false)) {
		  _Universe->cockpit[0]->savegame->SetPlayerLocation(myVec);
		}
		std::string st;
		if (SetPlayerSystem (st,false)) {
		  _Universe->cockpit[0]->savegame->SetStarSystem(st);
		}
      }
      vector <SavedUnits> saved=_Universe->cockpit[k]->savegame->ParseSaveGame (savegamefile,mysystem,mysystem,pos,setplayerXloc,credits,psu,k);
      playersaveunit.push_back(psu);
      _Universe->cockpit[k]->credits=credits;
  	  ss.push_back (_Universe->Init (mysystem,Vector(0,0,0),planetname));
	  if (setplayerXloc) {
	   	  playerNloc.push_back(pos);
	  }else {
		  playerNloc.push_back(QVector(FLT_MAX,FLT_MAX,FLT_MAX));
	  }
	  setplayerloc=setplayerXloc;//FIX ME will only set first player where he was

      for (unsigned int j=0;j<saved.size();j++) {
		savedun.push_back(saved[j]);
      }

    }
    SetStarSystemLoading (true);
    InitializeInput();

    vs_config->bindKeys();//gotta do this before we do ai

    createObjects(playersaveunit,ss,playerNloc);

    if (setplayerloc&&fighters) {
      if (fighters[0]) {
	//fighters[0]->SetPosition (Vector (0,0,0));
      }
    }
    
    while (!savedun.empty()) {
      AddUnitToSystem (&savedun.back());
      savedun.pop_back();
    }
    

    forcefeedback=new ForceFeedback();

    UpdateTime();
    delete SplashScreen;
    SplashScreen= NULL;
    SetStarSystemLoading (false);
    _Universe->LoadContrabandLists();
	{
		string str=vs_config->getVariable ("general","intro1","Welcome to Vega Strike! Use #8080FFTab#000000 to afterburn (#8080FF+,-#000000 cruise control), #8080FFarrows#000000 to steer.");
		if (!str.empty()) {
			UniverseUtil::IOmessage (0,"game","all",str);
			str=vs_config->getVariable ("general","intro2","The #8080FFt#000000 key targets objects; #8080FFspace#000000 fires at them & #8080FFa#000000 auto pilots there. Time");
			if (!str.empty()) {
				UniverseUtil::IOmessage (4,"game","all",str);
				str=vs_config->getVariable ("general","intro3","Compression: #8080FFF9; F10#000000 resets. Buy a jump drive & fly to a blue ball & press #8080FFj#000000");
				if (!str.empty()) {
					UniverseUtil::IOmessage (8,"game","all",str);
					str = vs_config->getVariable ("general","intro4","to warp to a near star. Target a base or planet & press #8080FF0#000000 to request landing");
					if (!str.empty()) {
						UniverseUtil::IOmessage (12,"game","all",str);
						str=vs_config->getVariable ("general","intro5","clearence. Inside the green box #8080FFd#000000 will land. When you launch, press #8080FFu#000000 to undock.");
						if (!str.empty())
							UniverseUtil::IOmessage (16,"game","all",str);
					}
				}
			}
		}
	}

	cout<<"Loading completed"<<endl;
	// Send a network msg saying we are ready
	if( Network!=NULL) {
		for( int l=0; l<numplayers; l++)
			Network[l].inGame();
	}

    _Universe->Loop(main_loop);
    ///return to idle func which now should call main_loop mohahahah
  }
  ///Draw Texture
  
} 


const char helpmessage[] =
"Command line options for vegastrike\n"
"\n"
" -D -d     Specify data directory\n"
" -M -m     Number of players\n"
" -S -s     Enable sound\n"
" -P -p     Specify player location\n"
" -A -A     Normal resolution (800x600)\n"
" -H -h     High resolution (1024x768)\n"
" -V -v     Super high resolution (1280x1024)\n"
"\n";

void ParseCommandLine(int argc, char ** lpCmdLine) {
  std::string st;
  QVector PlayerLocation;
  for (int i=1;i<argc;i++) {
    if(lpCmdLine[i][0]=='-') {
      switch(lpCmdLine[i][1]){
    case 'd':
    case 'D': {
      // Specifying data directory
        if(lpCmdLine[i][2] == 0) {
            cout << "Option -D requires an argument" << endl;
            exit(1);
        }
 		string datadir = &lpCmdLine[i][2];
        cout << "Using data dir " << datadir << endl;
        if(chdir(datadir.c_str())) {
            cout << "Error changing to specified data dir" << endl;
            exit(1);
         }
        }
	  case 'r':
      case 'R':
	break;
      case 'M':
      case 'm':
		  if (!(lpCmdLine[i][2]=='1'&&lpCmdLine[i][3]=='\0')) {
	  		CONFIGFILE=new char[40+strlen(lpCmdLine[i])+1];
			sprintf(CONFIGFILE,"vegastrike.config.%splayer",lpCmdLine[i]+2);
		  }
	break;
      case 'S':
      case 's':
	g_game.sound_enabled=1;
	break;
      case 'f':
      case 'F':
	break;
      case 'P':
      case 'p':
	sscanf (lpCmdLine[i]+2,"%lf,%lf,%lf",&PlayerLocation.i,&PlayerLocation.j,&PlayerLocation.k);
	SetPlayerLoc (PlayerLocation,true);
	break;
      case 'J':
      case 'j'://low rez
	st=string ((lpCmdLine[i])+2);
	SetPlayerSystem (st ,true);
	break;
      case 'A'://average rez
      case 'a': 
	g_game.y_resolution = 600;
	g_game.x_resolution = 800;
	break;
      case 'H':
      case 'h'://high rez
	g_game.y_resolution = 768;
	g_game.x_resolution = 1024;
	break;
      case 'V':
      case 'v':
	g_game.y_resolution = 1024;
	g_game.x_resolution = 1280;
	break;
      case 'G':
      case 'g':
	//viddrv = "GLDRV.DLL";
	break;
      case '-':
	// long options
	if(strcmp(lpCmdLine[i],"--benchmark")==0){
	  //benchmark=30.0;
	  benchmark=atof(lpCmdLine[i+1]);
	  i++;
	}
    else if(strcmp(lpCmdLine[i], "--help")==0) {
        cout << helpmessage;
        exit(0);
    }
      }
    }
    else{
      // no "-" before it - it's the mission name
      strncpy (mission_name,lpCmdLine[i],1023);
	  mission_name[1023]='\0';
    }
  }
  //FILE *fp = fopen("vid.cfg", "rb");
  //  GUID temp;
  //fread(&temp, sizeof(GUID), 1, fp);
  //fread(&temp, sizeof(GUID), 1, fp);
  //fread(&_ViewPortWidth, sizeof(DWORD), 1, fp);
  //fread(&_ViewPortHeight, sizeof(DWORD), 1, fp);
  //fread(&_ColDepth,sizeof(DWORD),1,fp);
  //fclose(fp);
}

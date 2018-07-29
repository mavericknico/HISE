/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/



namespace hise {
using namespace juce;


Array<juce::Identifier> HiseSettings::SettingFiles::getAllIds()
{
	Array<Identifier> ids;

#if USE_BACKEND
	ids.add(ProjectSettings);
	ids.add(UserSettings);
	ids.add(CompilerSettings);
	ids.add(GeneralSettings);
	ids.add(ScriptingSettings);
#endif
	ids.add(OtherSettings);
	ids.add(AudioSettings);
	ids.add(MidiSettings);

	return ids;
}

Array<juce::Identifier> HiseSettings::Project::getAllIds()
{
	Array<Identifier> ids;

	ids.add(Name);
	ids.add(Version);
	ids.add(Description);
	ids.add(BundleIdentifier);
	ids.add(PluginCode);
	ids.add(EmbedAudioFiles);
	ids.add(AdditionalDspLibraries);
	ids.add(OSXStaticLibs);
	ids.add(WindowsStaticLibFolder);
	ids.add(ExtraDefinitionsWindows);
	ids.add(ExtraDefinitionsOSX);
	ids.add(ExtraDefinitionsIOS);
	ids.add(AppGroupID);
	ids.add(RedirectSampleFolder);

	return ids;
}

Array<juce::Identifier> HiseSettings::Compiler::getAllIds()
{
	Array<Identifier> ids;

	ids.add(HisePath);
	ids.add(VisualStudioVersion);
	ids.add(UseIPP);

	return ids;
}

Array<juce::Identifier> HiseSettings::User::getAllIds()
{
	Array<Identifier> ids;

	ids.add(Company);
	ids.add(CompanyCode);
	ids.add(CompanyURL);
	ids.add(CompanyCopyright);
	ids.add(TeamDevelopmentID);

	return ids;
}

Array<juce::Identifier> HiseSettings::Scripting::getAllIds()
{
	Array<Identifier> ids;

	ids.add(EnableCallstack);
	ids.add(GlobalScriptPath);
	ids.add(CompileTimeout);
	ids.add(CodeFontSize);
	ids.add(EnableDebugMode);

	return ids;
}

Array<juce::Identifier> HiseSettings::Other::getAllIds()
{
	Array<Identifier> ids;

	ids.add(EnableAutosave);
	ids.add(AutosaveInterval);
	ids.add(AudioThreadGuardEnabled);

	return ids;
}

Array<juce::Identifier> HiseSettings::Midi::getAllIds()
{
	Array<Identifier> ids;

	ids.add(MidiInput);
	ids.add(MidiChannels);

	return ids;
}

Array<juce::Identifier> HiseSettings::Audio::getAllIds()
{
	Array<Identifier> ids;

	ids.add(Driver);
	ids.add(Device);
	ids.add(Output);
	ids.add(Samplerate);
	ids.add(BufferSize);

	return ids;
}

String getUncamelcasedId(const Identifier& id)
{
	auto n = id.toString();
	String pretty;
	auto ptr = n.getCharPointer();
	bool lastWasUppercase = true;

	while (!ptr.isEmpty())
	{
		if (ptr.isUpperCase() && !lastWasUppercase)
			pretty << " ";

		lastWasUppercase = ptr.isUpperCase();
		pretty << ptr.getAddress()[0];
		ptr++;
	}

	return pretty;
}


#define P(p) if (prop == p) { s << "### " << getUncamelcasedId(p) << "\n";
#define D(x) s << x << "\n";
#define P_() return s; } 

struct SettingDescription
{


	static String getDescription(const Identifier& prop)
	{
		String s;

		P(HiseSettings::Project::Name);
		D("The name of the project. This will be also the name of the plugin binaries");
		P_();

		P(HiseSettings::Project::Version);
		D("The version number of the project. Try using semantic versioning (`1.0.0`) for this.  ");
		D("The version number will be used to handle the user preset backward compatibility.");
		D("> Be aware that some hosts (eg. Logic) are very picky when they detect different plugin binaries with the same version.");
		P_();

		P(HiseSettings::Project::BundleIdentifier);
		D("This is a unique identifier used by Apple OS to identify the app. It must be formatted as reverse domain like this:");
		D("> `com.your-company.product`");
		P_();

		P(HiseSettings::Project::PluginCode);
		D("The code used to identify the plugins. This has to be four characters with the first one being uppercase like this:");
		D("> `Abcd`");
		P_();

		P(HiseSettings::Project::EmbedAudioFiles);
		D("If this is **enabled**, it will embed all audio files (impulse responses & loops) **as well as images** into the plugin.");
		D("This will not affect samples - they will always be streamed.  ");
		D("If it's **disabled**, it will use the resource files found in the app data directory and you need to make sure that your installer");
		D("copies them to the right location:");
		D("> **Windows:** `%APPDATA%\\Company\\Product\\`");
		D("> **macOS:** `~/Library/Application Support/Company/Product/`");
		D("Normally you would try to embed them into the binary, however if you have a lot of images and audio files (> 50MB)");
		D("the compiler will crash with an **out of heap space** error, so in this case you're better off not embedding them.");
		P_();

		P(HiseSettings::Project::AdditionalDspLibraries);
		D("If you have written custom DSP objects that you want to embed statically, you have to supply the class names of each DspModule class here");
		P_();

		P(HiseSettings::Project::WindowsStaticLibFolder);
		D("If you need to link a static library on Windows, supply the absolute path to the folder here. Unfortunately, relative paths do not work well with the VS Linker");
		P_();

		P(HiseSettings::Project::OSXStaticLibs);
		D("If you need to link a static library on macOS, supply the path to the .a library file here.");
		P_();

		P(HiseSettings::Project::ExtraDefinitionsWindows);
		D("This field can be used to add preprocessor definitions. Use it to tailor the compile options for HISE for the project.");
		D("#### Examples");
		D("```javascript");
		D("ENABLE_ALL_PEAK_METERS=0");
		D("NUM_POLYPHONIC_VOICES=100");
		D("```");
		P_();

		P(HiseSettings::Project::ExtraDefinitionsOSX);
		D("This field can be used to add preprocessor definitions. Use it to tailor the compile options for HISE for the project.");
		D("#### Examples");
		D("```javascript");
		D("ENABLE_ALL_PEAK_METERS=0");
		D("NUM_POLYPHONIC_VOICES=100");
		D("```");
		P_();

		P(HiseSettings::Project::ExtraDefinitionsIOS);
		D("This field can be used to add preprocessor definitions. Use it to tailor the compile options for HISE for the project.");
		D("#### Examples");
		D("```javascript");
		D("ENABLE_ALL_PEAK_METERS=0");
		D("NUM_POLYPHONIC_VOICES=100");
		D("```");
		P_();

		P(HiseSettings::Project::AppGroupID);
		D("If you're compiling an iOS app, you need to add an App Group to your Apple ID for this project and supply the name here.");
		D("App Group IDs must have reverse-domain format and start with group, like:");
		D("> `group.company.product`");
		P_();

		P(HiseSettings::Project::RedirectSampleFolder);
		D("You can use another location for your sample files. This is useful if you have limited space on your hard drive and need to separate the samples.");
		D("> HISE will create a file called `LinkWindows` / `LinkOSX` in the samples folder that contains the link to the real folder.");
		P_();

		P(HiseSettings::User::Company);
		D("Your company name. This will be used for the path to the app data directory so make sure you don't use weird characters here");
		P_();

		P(HiseSettings::User::CompanyCode);
		D("The unique code to identify your company. This must be 4 characters with the first one being uppercase like this:");
		D("> `Abcd`");
		P_();


		P(HiseSettings::User::TeamDevelopmentID);
		D("If you have a Apple Developer Account, enter the Developer ID here in order to code sign your app / plugin after compilation");
		P_();

		P(HiseSettings::Compiler::VisualStudioVersion);
		D("Set the VS version that you've installed. Make sure you always use the latest one, since I need to regularly deprecate the oldest version");
		P_();

		P(HiseSettings::Compiler::HisePath);
		D("This is the path to the source code of HISE. It must be the root folder of the repository (so that the folders `hi_core`, `hi_modules` etc. are immediate child folders.  ");
		D("This will be used for the compilation of the exported plugins and also contains all necessary SDKs (ASIO, VST, etc).");
		D("> Always make sure you are using the **exact** same source code that was used to build HISE or there will be unpredicatble issues.");
		P_();

		P(HiseSettings::Compiler::UseIPP);
		D("If enabled, HISE uses the FFT routines from the Intel Performance Primitive library (which can be downloaded for free) in order");
		D("to speed up the convolution reverb");
		D("> If you use the convolution reverb in your project, this is almost mandatory, but there are a few other places that benefit from having this library");
		P_();

		P(HiseSettings::Scripting::CodeFontSize);
		D("Changes the font size of the scripting editor. Beware that on newer versions of macOS, some font sizes will not be displayed (Please don't ask why...).  ");
		D("So if you're script is invisible, this might be the reason.");
		P_();

		P(HiseSettings::Scripting::EnableCallstack);
		D("This enables a stacktrace that shows the order of function calls that lead to the error (or breakpoint).");
		D("#### Example: ")
			D("```javascript");
		D("Interface: Breakpoint 1 was hit ");
		D(":  someFunction() - Line 5, column 18");
		D(":  onNoteOn() - Line 3, column 2");
		D("```");
		D("A breakpoint was set on the function `someFunction` You can see in the stacktrace that it was called in the `onNoteOn` callback.  ");
		D("Double clicking on the line in the console jumps to each location.");
		P_();

		P(HiseSettings::Scripting::CompileTimeout);
		D("Sets the timeout for the compilation of a script in **seconds**. Whenever the compilation takes longer, it will abort and show a error message.");
		D("This prevents hanging if you accidentally create endless loops like this:");
		D("```javascript");
		D("while(true)");
		D(" x++;");
		D("");
		D("```");
		P_();

		P(HiseSettings::Scripting::GlobalScriptPath);
		D("There is a folder that can be used to store global script files like additional API functions or generic UI widget definitions.");
		D("By default, this folder is stored in the application data folder, but you can choose to redirect it to another location, which may be useful if you want to put it under source control.");
		D("You can include scripts that are stored in this location by using the `{GLOBAL_SCRIPT_FOLDER}` wildcard:");
		D("```javascript");
		D("// Includes 'File.js'");
		D("include(\"{GLOBAL_SCRIPT_FOLDER}File.js\");");
		D("```");
		P_();

		P(HiseSettings::Scripting::EnableDebugMode);
		D("This enables the debug logger which creates a log file containing performance issues and system specifications.");
		D("It's the same functionality as found in the compiled plugins.");
		P_();

		P(HiseSettings::Other::EnableAutosave);
		D("The autosave function will store up to 5 archive files called `AutosaveXXX.hip` in the archive folder of the project.");
		D("In a rare and almost never occuring event of a crash, this might be your saviour...");
		P_();

		P(HiseSettings::Other::AutosaveInterval);
		D("The interval for the autosaver in minutes. This must be a number between `1` and `30`.");
		P_();

		P(HiseSettings::Other::AudioThreadGuardEnabled);
		D("Watches for illegal calls in the audio thread. Use this during script development to catch allocations etc.");
		P_();

		return s;

	};
};

#undef P
#undef D
#undef P_


HiseSettings::Data::Data(MainController* mc_) :
	mc(mc_),
	data("SettingRoot")
{
	for (const auto& id : SettingFiles::getAllIds())
		data.addChild(ValueTree(id), -1, nullptr);

	loadDataFromFiles();
}

juce::File HiseSettings::Data::getFileForSetting(const Identifier& id) const
{
	
	auto appDataFolder = NativeFileHandler::getAppDataDirectory();

	if (id == SettingFiles::AudioSettings)		return appDataFolder.getChildFile("DeviceSettings.xml");
	else if (id == SettingFiles::MidiSettings)		return appDataFolder.getChildFile("DeviceSettings.xml");
	else if (id == SettingFiles::GeneralSettings)	return appDataFolder.getChildFile("GeneralSettings.xml");

#if USE_BACKEND

	auto handler = &GET_PROJECT_HANDLER(mc->getMainSynthChain());

	if (id == SettingFiles::ProjectSettings)	return handler->getWorkDirectory().getChildFile("project_info.xml");
	else if (id == SettingFiles::UserSettings)		return handler->getWorkDirectory().getChildFile("user_info.xml");
	else if (id == SettingFiles::CompilerSettings)	return appDataFolder.getChildFile("compilerSettings.xml");
	else if (id == SettingFiles::ScriptingSettings)	return appDataFolder.getChildFile("ScriptSettings.xml");
	else if (id == SettingFiles::OtherSettings)		return appDataFolder.getChildFile("OtherSettings.xml");

	jassertfalse;

#endif
	
	return File();
}

void HiseSettings::Data::loadDataFromFiles()
{
	for (const auto& id : SettingFiles::getAllIds())
		loadSettingsFromFile(id);
}

void HiseSettings::Data::refreshProjectData()
{
	loadSettingsFromFile(SettingFiles::ProjectSettings);
	loadSettingsFromFile(SettingFiles::UserSettings);
}

void HiseSettings::Data::loadSettingsFromFile(const Identifier& id)
{
	auto f = getFileForSetting(id);

	ValueTree v = ConversionHelpers::loadValueTreeFromFile(f, id);

	if (!v.isValid())
		v = ValueTree(id);

	data.removeChild(data.getChildWithName(id), nullptr);
	data.addChild(v, -1, nullptr);

	addMissingSettings(v, id);
}

var HiseSettings::Data::getSetting(const Identifier& id) const
{
	for (const auto& c : data)
	{
		auto prop = c.getChildWithName(id);

		static const Identifier va("value");

		if (prop.isValid())
		{
			auto value = prop.getProperty(va);

			if (value == "Yes")
				return var(true);

			if (value == "No")
				return var(false);

			return value;
		}
	}

	return var();
}

void HiseSettings::Data::addSetting(ValueTree& v, const Identifier& id)
{
	if (v.getChildWithName(id).isValid())
		return;

	ValueTree child(id);
	child.setProperty("value", getDefaultSetting(id), nullptr);
	v.addChild(child, -1, nullptr);
}

juce::StringArray HiseSettings::Data::getOptionsFor(const Identifier& id)
{
	if (id == Project::EmbedAudioFiles ||
		id == Compiler::UseIPP ||
		id == Scripting::EnableCallstack ||
		id == Other::EnableAutosave ||
		id == Scripting::EnableDebugMode ||
		id == Other::AudioThreadGuardEnabled) 
		return { "Yes", "No" };

	if (id == Compiler::VisualStudioVersion)
		return { "Visual Studio 2015", "Visual Studio 2017" };

#if IS_STANDALONE_APP
	else if (Audio::getAllIds().contains(id))
	{
		auto manager = dynamic_cast<AudioProcessorDriver*>(mc)->deviceManager;
		StringArray sa;

		if (id == Audio::Driver)
		{
			const auto& list = manager->getAvailableDeviceTypes();
			for (auto l : list)
				sa.add(l->getTypeName());

		}
		else if (id == Audio::Device)
		{
			const auto currentDevice = manager->getCurrentDeviceTypeObject();
			return currentDevice->getDeviceNames();
		}
		else if (id == Audio::BufferSize)
		{
			const auto currentDevice = manager->getCurrentAudioDevice();
			const auto& bs = ConversionHelpers::getBufferSizesForDevice(currentDevice);

			for (auto l : bs)
				sa.add(String(l));
		}
		else if (id == Audio::Samplerate)
		{
			const auto currentDevice = manager->getCurrentAudioDevice();
			const auto& bs = ConversionHelpers::getSampleRates(currentDevice);

			for (auto l : bs)
				sa.add(String(roundDoubleToInt(l)));
		}
		else if (id == Audio::Output)
		{
			const auto currentDevice = manager->getCurrentAudioDevice();
			return ConversionHelpers::getChannelPairs(currentDevice);
		}
		return sa;
	}
	else if (id == Midi::MidiInput)
	{
		return MidiInput::getDevices();
	}
#endif
	else if (id == Midi::MidiChannels)
	{
		return ConversionHelpers::getChannelList();
	}


	return {};
}

bool HiseSettings::Data::isFileId(const Identifier& id)
{
	return id == Compiler::HisePath || id == Scripting::GlobalScriptPath || id == Project::RedirectSampleFolder;
}


bool HiseSettings::Data::isToggleListId(const Identifier& id)
{
	return id == Midi::MidiInput;
}

void HiseSettings::Data::addMissingSettings(ValueTree& v, const Identifier &id)
{
	Array<Identifier> ids;

	if (id == SettingFiles::ProjectSettings)		ids = Project::getAllIds();
	else if (id == SettingFiles::UserSettings)		ids = User::getAllIds();
	else if (id == SettingFiles::CompilerSettings)	ids = Compiler::getAllIds();
	else if (id == SettingFiles::ScriptingSettings) ids = Scripting::getAllIds();
	else if (id == SettingFiles::OtherSettings)		ids = Other::getAllIds();

	for (const auto& id_ : ids)
		addSetting(v, id_);
}

juce::AudioDeviceManager* HiseSettings::Data::getDeviceManager()
{
	return dynamic_cast<AudioProcessorDriver*>(mc)->deviceManager;
}

void HiseSettings::Data::initialiseAudioDriverData(bool forceReload/*=false*/)
{
	ignoreUnused(forceReload);

#if IS_STANDALONE_APP
	static const Identifier va("value");

	auto v = data.getChildWithName(SettingFiles::AudioSettings);
	
	for (const auto& id : Audio::getAllIds())
	{
		if (forceReload)
			v.getChildWithName(id).setProperty(va, getDefaultSetting(id), nullptr);
		else
			addSetting(v, id);
	}

	auto v2 = data.getChildWithName(SettingFiles::MidiSettings);
	
	for (const auto& id : Midi::getAllIds())
	{
		if (forceReload)
			v2.getChildWithName(id).setProperty(va, getDefaultSetting(id), nullptr);
		else
			addSetting(v2, id);
	}

#endif


}



var HiseSettings::Data::getDefaultSetting(const Identifier& id)
{
	BACKEND_ONLY(auto& handler = GET_PROJECT_HANDLER(mc->getMainSynthChain()));

	if (id == Project::Name)
	{
		BACKEND_ONLY(return handler.getWorkDirectory().getFileName());
	}
	else if (id == Project::Version)			    return "1.0.0";
	else if (id == Project::BundleIdentifier)	    return "com.myCompany.product";
	else if (id == Project::PluginCode)			    return "Abcd";
	else if (id == Project::EmbedAudioFiles)		return "Yes";
	else if (id == Project::RedirectSampleFolder)	BACKEND_ONLY(return handler.isRedirected(ProjectHandler::SubDirectories::Samples) ? handler.getSubDirectory(ProjectHandler::SubDirectories::Samples).getFullPathName() : "");
	else if (id == Other::EnableAutosave)			return "Yes";
	else if (id == Other::AutosaveInterval)			return 5;
	else if (id == Other::AudioThreadGuardEnabled)  return "Yes";
	else if (id == Scripting::CodeFontSize)			return 17.0;
	else if (id == Scripting::EnableCallstack)		return "No";
	else if (id == Scripting::CompileTimeout)		return 5.0;
	else if (id == Compiler::VisualStudioVersion)	return "Visual Studio 2017";
	else if (id == Compiler::UseIPP)				return "Yes";
	else if (id == User::CompanyURL)				return "http://yourcompany.com";
	else if (id == User::CompanyCopyright)			return "(c)2017, Company";
	else if (id == User::CompanyCode)				return "Abcd";
	else if (id == User::Company)					return "My Company";
	else if (id == Scripting::GlobalScriptPath)		
	{
		FRONTEND_ONLY(jassertfalse);

		File scriptFolder = File(NativeFileHandler::getAppDataDirectory()).getChildFile("scripts");
		if (!scriptFolder.isDirectory())
			scriptFolder.createDirectory();

		return scriptFolder.getFullPathName();
	}
	else if (id == Scripting::EnableDebugMode)		return mc->getDebugLogger().isLogging() ? "Yes" : "No";
	else if (id == Audio::Driver)					return getDeviceManager()->getCurrentAudioDeviceType();
	else if (id == Audio::Device)
	{
		auto device = dynamic_cast<AudioProcessorDriver*>(mc)->deviceManager->getCurrentAudioDevice();
		return device != nullptr ? device->getName() : "No Device";
	}
	else if (id == Audio::Output)
	{
		auto device = dynamic_cast<AudioProcessorDriver*>(mc)->deviceManager->getCurrentAudioDevice();

		return ConversionHelpers::getCurrentOutputName(device);
	}
	else if (id == Audio::Samplerate)				return dynamic_cast<AudioProcessorDriver*>(mc)->getCurrentSampleRate();
	else if (id == Audio::BufferSize)				return dynamic_cast<AudioProcessorDriver*>(mc)->getCurrentBlockSize();
	else if (id == Midi::MidiInput)					return dynamic_cast<AudioProcessorDriver*>(mc)->getMidiInputState().toInt64();
	else if (id == Midi::MidiChannels)
	{
		auto state = BigInteger(dynamic_cast<AudioProcessorDriver*>(mc)->getChannelData());
		auto firstSetBit = state.getHighestBit();
		return ConversionHelpers::getChannelList()[firstSetBit];
	}

	return var();
}

juce::Result HiseSettings::Data::checkInput(const Identifier& id, const var& newValue)
{
	if (id == Other::AutosaveInterval && !TestFunctions::isValidNumberBetween(newValue, { 1.0f, 30.0f }))
		return Result::fail("The autosave interval must be between 1 and 30 minutes");

	if (id == Project::Version)
	{
		const String version = newValue.toString();
		SemanticVersionChecker versionChecker(version, version);

		if (!versionChecker.newVersionNumberIsValid())
		{
			return Result::fail("The version number is not a valid semantic version number. Use something like 1.0.0.\n " \
				"This is required for the user presets to detect whether it should ask for updating the presets after a version bump.");
		};
	}

	if (id == Project::AppGroupID || id == Project::BundleIdentifier)
	{
		const String wildcard = (id == HiseSettings::Project::BundleIdentifier) ?
			R"(com\.[\w\d-_]+\.[\w\d-_]+$)" :
			R"(group\.[\w\d-_]+\.[\w\d-_]+$)";

		if (!RegexFunctions::matchesWildcard(wildcard, newValue.toString()))
		{
			return Result::fail(id.toString() + " doesn't match the required format.");
		}
	}

	if (id == Project::PluginCode || id == User::CompanyCode)
	{
		const String pluginCode = newValue.toString();
		const String codeWildcard = "[A-Z][a-z][a-z][a-z]";

		if (pluginCode.length() != 4 || !RegexFunctions::matchesWildcard(codeWildcard, pluginCode))
		{
			return Result::fail("The code doesn't match the required formula. Use something like 'Abcd'\n" \
				"This is required for exported AU plugins to pass the AU validation.");
		};
	}

	if (id == Project::Name || id == User::Company)
	{
		const String name = newValue.toString();

		if (!name.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 _-"))
		{
			return Result::fail("Illegal Project name\n" \
				"The Project name must not contain exotic characters");
		}

		if (name.isEmpty())
			return Result::fail("The project name / company name must not be empty");
	}

	if (id == Compiler::HisePath)
	{
		File f = File(newValue.toString());

		if (!f.isDirectory())
			return Result::fail("The HISE path is not a valid directory");

		if (!f.getChildFile("hi_core").isDirectory())
			return Result::fail("The HISE path does not contain the HISE source code");
	}

	if (id == Scripting::GlobalScriptPath && !File(newValue.toString()).isDirectory())
		return Result::fail("The global script folder is not a valid directory");

	return Result::ok();
}

void HiseSettings::Data::settingWasChanged(const Identifier& id, const var& newValue)
{
	if (id == Project::RedirectSampleFolder)
	{
#if USE_BACKEND
		auto& handler = GET_PROJECT_HANDLER(mc->getMainSynthChain());

		if (File::isAbsolutePath(newValue.toString()))
			handler.createLinkFile(ProjectHandler::SubDirectories::Samples, File(newValue.toString()));
		else
			ProjectHandler::getLinkFile(handler.getWorkDirectory().getChildFile("Samples")).deleteFile();
#endif
	}

	if (id == Scripting::EnableCallstack)
		mc->updateCallstackSettingForExistingScriptProcessors();

	else if (id == Scripting::CodeFontSize)
		mc->getFontSizeChangeBroadcaster().sendChangeMessage();

	else if (id == Other::EnableAutosave || id == Other::AutosaveInterval)
		mc->getAutoSaver().updateAutosaving();
	else if (id == Other::AudioThreadGuardEnabled)
		mc->getKillStateHandler().enableAudioThreadGuard(newValue);

	else if (id == Scripting::EnableDebugMode)
		newValue ? mc->getDebugLogger().startLogging() : mc->getDebugLogger().stopLogging();
	else if (id == Audio::Samplerate)
		dynamic_cast<AudioProcessorDriver*>(mc)->setCurrentSampleRate(newValue.toString().getDoubleValue());
	else if (id == Audio::BufferSize)
		dynamic_cast<AudioProcessorDriver*>(mc)->setCurrentBlockSize(newValue.toString().getIntValue());
	else if (id == Audio::Driver)
	{
		auto driver = dynamic_cast<AudioProcessorDriver*>(mc);
		driver->deviceManager->setCurrentAudioDeviceType(newValue.toString(), true);
		auto device = driver->deviceManager->getCurrentAudioDevice();

		if (device == nullptr)
		{
			PresetHandler::showMessageWindow("Error initialising driver", "The audio driver could not be opened. The default settings will be loaded.", PresetHandler::IconType::Error);
			driver->resetToDefault();
		}

		initialiseAudioDriverData(true);
		sendChangeMessage();
	}
	else if (id == Audio::Output)
	{
		auto driver = dynamic_cast<AudioProcessorDriver*>(mc);
		auto device = driver->deviceManager->getCurrentAudioDevice();
		auto list = ConversionHelpers::getChannelPairs(device);
		auto outputIndex = list.indexOf(newValue.toString());

		if (outputIndex != -1)
		{
			AudioDeviceManager::AudioDeviceSetup config;
			driver->deviceManager->getAudioDeviceSetup(config);

			auto& original = config.outputChannels;

			original.clear();
			original.setBit(outputIndex * 2, 1);
			original.setBit(outputIndex * 2 + 1, 1);

			config.useDefaultOutputChannels = false;

			driver->deviceManager->setAudioDeviceSetup(config, true);
		}

		
	}
	else if (id == Audio::Device)
	{
		auto driver = dynamic_cast<AudioProcessorDriver*>(mc);
		driver->setAudioDevice(newValue.toString());

		auto device = driver->deviceManager->getCurrentAudioDevice();

		if (device == nullptr)
		{
			PresetHandler::showMessageWindow("Error initialising driver", "The audio driver could not be opened. The default settings will be loaded.", PresetHandler::IconType::Error);
			driver->resetToDefault();
		}

		initialiseAudioDriverData(true);
		sendChangeMessage();
	}
	else if (id == Midi::MidiInput)
	{
		auto state = BigInteger((int64)newValue);
		auto driver = dynamic_cast<AudioProcessorDriver*>(mc);

		auto midiNames = MidiInput::getDevices();

		for (int i = 0; i < midiNames.size(); i++)
			driver->toggleMidiInput(midiNames[i], state[i]);
	}
	else if (id == Midi::MidiChannels)
	{
		auto sa = HiseSettings::ConversionHelpers::getChannelList();
		auto index = sa.indexOf(newValue.toString());

		BigInteger s = 0;
		s.setBit(index, true);

		auto intValue = s.toInteger();
		auto channelData = mc->getMainSynthChain()->getActiveChannelData();

		channelData->restoreFromData(intValue);
	}
}

bool HiseSettings::Data::TestFunctions::isValidNumberBetween(var value, Range<float> range)
{
	auto number = value.toString().getFloatValue();

	if (std::isnan(number))
		return false;

	if (std::isinf(number))
		return false;

	number = FloatSanitizers::sanitizeFloatNumber(number);

	return range.contains(number);
}

juce::StringArray HiseSettings::ConversionHelpers::getChannelPairs(AudioIODevice* currentDevice)
{
	if (currentDevice != nullptr)
	{
		StringArray items = currentDevice->getOutputChannelNames();

		StringArray pairs;

		for (int i = 0; i < items.size(); i += 2)
		{
			const String& name = items[i];

			if (i + 1 >= items.size())
				pairs.add(name.trim());
			else
				pairs.add(getNameForChannelPair(name, items[i + 1]));
		}

		return pairs;
	}

	return StringArray();
}

juce::String HiseSettings::ConversionHelpers::getNameForChannelPair(const String& name1, const String& name2)
{
	String commonBit;

	for (int j = 0; j < name1.length(); ++j)
		if (name1.substring(0, j).equalsIgnoreCase(name2.substring(0, j)))
			commonBit = name1.substring(0, j);

	// Make sure we only split the name at a space, because otherwise, things
	// like "input 11" + "input 12" would become "input 11 + 2"
	while (commonBit.isNotEmpty() && !CharacterFunctions::isWhitespace(commonBit.getLastCharacter()))
		commonBit = commonBit.dropLastCharacters(1);

	return name1.trim() + " + " + name2.substring(commonBit.length()).trim();
}

juce::String HiseSettings::ConversionHelpers::getCurrentOutputName(AudioIODevice* currentDevice)
{
	if(currentDevice != nullptr)
	{
		auto list = getChannelPairs(currentDevice);
		const int thisOutputName = (currentDevice->getActiveOutputChannels().getHighestBit() - 1) / 2;

		return list[thisOutputName];
	}

	return "";
}

}

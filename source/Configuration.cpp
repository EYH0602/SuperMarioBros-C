#include <fstream>

#include "Configuration.hpp"

/**
 * List of all supported configuration options.
 */
std::list<ConfigurationOption*> Configuration::configurationOptions = {
    &Configuration::audioFrequency,
    &Configuration::frameRate,
    &Configuration::renderScale,
    &Configuration::romFileName
};

/**
 * Audio frequency, in Hz
 */
BasicConfigurationOption<int> Configuration::audioFrequency(
    "audio.frequency", 48000
);

/**
 * Frame rate (per second).
 */
BasicConfigurationOption<int> Configuration::frameRate(
    "game.frame_rate", 60
);

/**
 * Scaling factor for rendering.
 */
BasicConfigurationOption<int> Configuration::renderScale(
    "video.scale", 3
);

/**
 * Filename for the SMB ROM image.
 */
BasicConfigurationOption<std::string> Configuration::romFileName(
    "game.rom_file", "Super Mario Bros. (JU) (PRG0) [!].nes"
);

ConfigurationOption::ConfigurationOption(
    const std::string& path) :
    path(path)
{
}

const std::string& ConfigurationOption::getPath() const
{
    return path;
}

void Configuration::initialize(const std::string& fileName)
{
    // Check that the configuration file exists.
    // If it does not exist, we will fall back to default values.
    //
    std::ifstream configFile(fileName.c_str());

    if (configFile.good())
    {
        // Load the configuration file into a property tree to parse it
        //
        boost::property_tree::ptree propertyTree;
        boost::property_tree::ini_parser::read_ini(configFile, propertyTree);

        // Try to load the value for all known config options
        //
        for (auto option : configurationOptions)
        {
            option->initializeValue(propertyTree);
        }
    }
}

int Configuration::getAudioFrequency()
{
    return audioFrequency.getValue();
}

int Configuration::getFrameRate()
{
    return frameRate.getValue();
}

int Configuration::getRenderScale()
{
    return renderScale.getValue();
}

const std::string& Configuration::getRomFileName()
{
    return romFileName.getValue();
}
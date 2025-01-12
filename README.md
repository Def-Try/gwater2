# GWater 2 Server
## Description
A simple bot that was used to serve watermarked files and unique installers that could be used to identify a discord used, if files were found in public.
This does NOT use any personal info, such as IPs and required customer to join discord server.
Made unneeded with GWater 2 1.0 release and opensourcing. Oh well. Might as well opensource the bot.
And we do NOT talk about the 0.3b. Never again.

## Command list
- /download - starts download generation process. the only command that can be used not only by admins
- /kbid - kills the bot
- /cc - clears cooldown list
- /rbid - removes blacklisted id
- /abid - adds blacklisted id
- /lbid - lists all blacklisted ids

## What it actually does
When user requests files with /download slash command:
- Bot checks if user is Mee (meetric), is in admins list, has any supported patreon roles, or if test mode is enabled. If this check fails, a public message gets sent about lack of user persmissions to use it.
- Bot checks if user was manually blacklisted by an admin. If so, a public message gets sent about "error code" `ERR_UID_BLACKLISTED`
- Bot checks if user has already had a successful download in the last 15 minutes. If so, an ephemeral message gets sent about it telling user to wait X more minutes.
- Bot sends an ephemeral message telling that it's generating files.
- Bot tries 5 times to generate file zip - binary files get watermarked, everything gets zipped together and gets sent to a special channel. If that fails, message gets edited telling that an error occured.
- Bot edits an ephemeral message to tell that it's now generating an installer.
- Bot tries 5 times to generate installer bat file - proper files download link is inserted into batch file and it gets sent to a special channel. If that fails, message gets edited telling that an error occured.
- Bot finally edits an ephemeral message to provide both installer and direct files download links.

## Watermarking
Watermarking this bot does is simply appending 8 bytes of downloader discord id to the end of gwater 2 binary files. This is fairly easy to do, and to discover due to vast amount of zeros towards the DLL file end (damn you windows padding!!), but wasn't a problem somehow. To identify the downloader from files you just need to take last 8 bytes of any gwater2 module dll and read it as a little-endian integer.

## Configuration
`gw2/` *needs* to contain at least 3 files to function properly:
- garrysmod/lua/bin/gmcl_gwater2_main_win32.dll
- garrysmod/lua/bin/gmcl_gwater2_win32.dll
- garrysmod/lua/bin/gmcl_gwater2_win64.dll
  
These are the files that get watermarked and if any of these don't exist, the download will fail.

Most of bot configuration is stored in `config/` directory and is dictated by 5 files.
- admins.json: admins user ids list
- blacklist.json: blacklisted user ids list. changed by bot with some commands!
- files_channel.txt: channel where generated file zips and installers get sent.
- meeid.txt: meetric's user id
- patrons.json: patrons role ids file
- token.txt: discord bot token
  
Some of the configuration, however, needs to be changed in main.py directly:
- TEST_MODE: Is bot in test mode? True means that no UID checks will be done.
- COOLDOWN: How much should we wait before letting user to generate new download? In seconds.
- GENERAL_ID: General channel / where the startup message should be sent.
- STARTUP_MSG: Startup message

import discord
import requests
import time
import json
import shutil
import random
import os

###############################################################################
# Configuration
# Edit these variables (files) to config GW2 server bot
###############################################################################

# Bot token.
# loaded from "config/token.txt"
with open("config/token.txt", 'r', encoding="UTF-8") as token_file:
    TOKEN = token_file.read()

# Patron role IDs.
# loaded from "config/patrons.json"
with open("config/patrons.json", 'r', encoding="UTF-8") as patrons_file:
    PATRON_ROLES = json.load(patrons_file)

# Admin access discord UIDs
# loaded from "config/admins.json"
with open("config/admins.json", 'r', encoding="UTF-8") as admins_file:
    ADMINS = json.load(admins_file)

# Meetric's discord UID.
# loaded from "config/meeid.txt"
with open("config/meeid.txt", 'r', encoding="UTF-8") as meeid_file:
    MEE = int(meeid_file.read())

# discord IDs blacklisted from downloading GW2
# loaded from "config/blacklist.json"
with open("config/blacklist.json", 'r', encoding="UTF-8") as blacklist_file:
    BLACKLISTED = json.load(blacklist_file)

# channel id for saving gw2 files
# loaded from "config/files_channel.txt"
with open("config/files_channel.txt", 'r', encoding="UTF-8") as fileschan_file:
    FILES_CHANNEL = int(fileschan_file.read())

# Is bot in test mode?
# True means that no UID checks will be done.
# Only use for test bots that don't have access to "outside" "world"
TEST_MODE = False

# How much should we wait before letting user to generate new download?
# In seconds.
COOLDOWN = 60*15 # 15m

# General channel / where the startup message should be sent.
GENERAL_ID = 962140720699961356 # A.I.D.S. #general

# Startup message
STARTUP_MSG = "I. LIVE. AGAIN."

###############################################################################
# End of configuration
# Do not go further if you don't know what you are doing
###############################################################################

bot = discord.Bot()

@bot.event
async def on_ready():
    print("Running...")

    channel = bot.get_channel(GENERAL_ID)
    await channel.send(STARTUP_MSG)

    if TEST_MODE:
        print("RUNNING IN TEST MODE")
        print("This means that no User ID checks are done.")
        print("If you see that message in production environment, ")
        print("KILL BOT APPLICATION AND SET TEST_MODE TO FALSE")

cooldowns = {}

# Bot control commands.
# Shortened to two-four letter sequences to make it impossible to guess what they mean

# Kill Bot. In case if something goes terribly wrong.
@bot.slash_command(description="K.B.")
async def kb(ctx):
    if ctx.author.id not in ADMINS:
        await ctx.respond("Not allowed.", ephemeral=True)
        return
    await ctx.respond("Bye!", ephemeral=True)
    exit(1)

# Clear Cooldowns.
@bot.slash_command(description="C.C.")
async def cc(ctx):
    global cooldowns
    if ctx.author.id not in ADMINS:
        await ctx.respond("Not allowed.", ephemeral=True)
        return
    cooldowns = {}
    await ctx.respond("Successfully reset all cooldowns", ephemeral=True)

# Remove Blacklisted ID. Blacklisted IDs can't use /download
@bot.slash_command(description="R.B.ID.")
async def rbid(ctx, uid: str):
    global BLACKLISTED
    if ctx.author.id not in ADMINS:
        await ctx.respond("Not allowed.", ephemeral=True)
        return
    uid = int(uid)
    BLACKLISTED.remove(uid)
    with open("config/blacklist.json", 'w', encoding="UTF-8") as blacklist_file:
        json.dump(BLACKLISTED, blacklist_file)
    await ctx.respond(f"Removed {uid} (<@{uid}>) from blacklisted IDs", ephemeral=True)

# List Blacklisted IDs. Blacklisted IDs can't use /download
@bot.slash_command(description="L.B.ID.")
async def lbid(ctx):
    global BLACKLISTED
    if ctx.author.id not in ADMINS:
        await ctx.respond("Not allowed.", ephemeral=True)
        return
    blids = "Blacklisted IDs:\n"
    for uid in BLACKLISTED:
        blids += f"{uid} / <@{uid}>\n"
    blids = blids.strip()
    await ctx.respond(blids, ephemeral=True)

# Add Blacklisted IDs. Blacklisted IDs can't use /download
@bot.slash_command(description="A.B.ID.")
async def abid(ctx, uid: str):
    global BLACKLISTED
    if ctx.author.id not in ADMINS:
        await ctx.respond("Not allowed.", ephemeral=True)
        return
    uid = int(uid)
    BLACKLISTED += [uid]
    with open("config/blacklist.json", 'w', encoding="UTF-8") as blacklist_file:
        json.dump(BLACKLISTED, blacklist_file)
    await ctx.respond(f"Added {uid} (<@{uid}>) to blacklisted IDs", ephemeral=True)

# Generates GWater **files**
async def generate_gw2f(ctx):
    channel = bot.get_channel(FILES_CHANNEL)
    to_patch = [
        "garrysmod/lua/bin/gmcl_gwater2_main_win32.dll",
        "garrysmod/lua/bin/gmcl_gwater2_win32.dll",
        "garrysmod/lua/bin/gmcl_gwater2_win64.dll"]
    shutil.copytree("gw2", "__temp__")
    for f in to_patch:
        with open("__temp__/"+f, "ab") as fp:
            fp.write(ctx.author.id.to_bytes(8, 'little'))
    files = "files_"+str(random.randint(10000, 99999))
    shutil.make_archive(files, "zip", "__temp__")
    shutil.rmtree("__temp__")
    for i in range(5):
        try:
            _ = (await channel.send(f"Generated files! attempt #{i}\n"+
                  "Watermarked: `"+(' '.join(f"{i:02x}" for i in ctx.author.id.to_bytes(8, 'little')))+"`\n"+
                  f"UID: {ctx.author.id}\nIssuer: {ctx.author.mention}", file=discord.File(files+".zip"))).attachments[0].url
            os.remove(files+".zip")
            return _
        except:
            pass
    return None


# Generates GWater **installer**
async def generate_installer(ctx, filesurl):
    channel = bot.get_channel(FILES_CHANNEL)
    file = "installer_"+str(random.randint(10000, 99999))
    with open("gwater2_install.bat", "r") as rfp:
        with open(file+".bat", "w") as wfp:
            wfp.write(rfp.read().format(link=filesurl.replace("&", '"&"')))
    for i in range(5):
        try:
            _ = (await channel.send(f"Generated installer! attempt #{i}\n"+
              f"UID: {ctx.author.id}\nIssuer: {ctx.author.mention}\nFiles URL: {filesurl}", file=discord.File(file+".bat"))).attachments[0].url
            os.remove(file+".bat")
            return _
        except:
            pass
    return None


@bot.slash_command(description="Generate and send installer download link. \n"
                               "Available only 1 time each 15 minutes.!")
async def download(ctx):
    if ctx.author.id in BLACKLISTED:
        await ctx.respond("Server side error occured.\n"
                          "Ping @googer_ about error code:\n"
                          "`ERR_UID_BLACKLISTED` - Explicit blacklist.")
        return

    if cooldowns.get(ctx.author.id, 0) > time.time():
        await ctx.respond(f"Please wait before generating new download link. "
                          f"Try again in <t:{round(cooldowns.get(ctx.author.id))}:R>",
                          ephemeral=True)
        return

    if ( not hasattr(ctx.author, "roles") or
         not any(role.id in PATRON_ROLES for role in ctx.author.roles) ) and \
         not ctx.author.id == MEE and \
         not TEST_MODE:
        await ctx.respond("You are not permitted to run that command.")
        return

    await ctx.respond("Generating GW2 files...", ephemeral=True)
    filesurl = await generate_gw2f(ctx)
    if filesurl == None:
        await ctx.interaction.edit_original_response(content=
            f"Failed to generate files after 5 attempts! Try again.")
        return
    
    await ctx.interaction.edit_original_response(content=
        f"Generating installer...")
    
    instlurl = await generate_installer(ctx, filesurl)
    if instlurl == None:
        await ctx.interaction.edit_original_response(content=
            f"Failed to generate installer after 5 attempts! Try again.")
        return

    cooldowns[ctx.author.id] = time.time() + COOLDOWN
    await ctx.interaction.edit_original_response(content=
        f"Click [`here`](<{instlurl}>) to download GW2 installer.\n"+\
        f"Click [`here`](<{filesurl}>) to download GW2 files in case installer fails.")
bot.run(TOKEN)

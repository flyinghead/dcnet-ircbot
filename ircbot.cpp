/*
	IRC bot for Worms World Party, Starlancer and The Next Tetris.
    Copyright (C) 2025  Flyinghead

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <libircclient.h>
#include <libirc_rfcnumeric.h>
#include <iterator>
#include <cstring>
#include <dcserver/discord.hpp>
#include <dcserver/status.hpp>
#include <map>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>

const char *BOT_NAME = "DCNetBot";

const char *STAR_CHAN = "#GSP!slancerdc";
const char *TET_CHAN = "#TNT_Lobby";

const char *Channels[] {
	"#Aerial",
	"#Retro",
	"#Tankxz",
	"#Tournament",
	"#Pro",
	"#Strategy",
	"#Drops_only",
	"#Blast_Zone",
	"#ClockWorm_Orange",
	"#Arms_Race",
	"#High_Explosive",
	"#FullRope",
	"#Sudden_Sinking",
	"#Men_at_Worm",
	"#Artillery",
	"#Armageddon",
	"#Kung_Food",
	STAR_CHAN,
	TET_CHAN,
};

const char *Topics[std::size(Channels)] {
	"2 Worms are equipped with Jet Packs",
	"23 The original Worms default scheme",
	"29 Immobile worms, a battle to the death",
	"30 For the more advanced players, who prefer the more difficult weapons",
	"22 Manual Worm placement and Scheme for the more advanced player",
	"",
	"12 All weapons are dropped via crate",
	"8 Powerful weapons are provided. High worm energy makes a good battle",
	"10 A variety of hugely powerful weapons, set on delay",
	"5 Hugely powerful weapons released gradually as the battle progresses",
	"17 Highly explosive weapons for good destruction",
	"15 For players who like to use the rope to good effect",
	"28 With instant water rise",
	"20 Defend yourself before it is too late",
	"6 Like Tankxz",
	"4 Instant nuclear explosion leaves your Worms with health problems",
	"",
};

std::string curTopics[std::size(Channels)];

unsigned joinIndex;
unsigned topicIndex;

// protects collections below so that the status updater thread can read their size safely
std::mutex mutex;
using Lock = std::unique_lock<std::mutex>;

// Worms World Party
std::map<std::string, std::string> wwpPlayers;
struct Game
{
	std::string gameId;
	std::string creator;
	std::string channel;
	std::vector<std::string> players;
};
std::map<std::string, Game> wwpGames;
// StarLancer
std::set<std::string> starlancerPlayers;
// Next tetris
std::set<std::string> tetrisPlayers;

static void discordWwpPlayerJoined(const char *nick, const char *channel)
{
	Notif notif;
	notif.content = "Player **" + discordEscape(nick) + "** has joined channel **" + std::string(channel) + "**";
	notif.embed.title = "Players";
	for (auto& [player, channel] : wwpPlayers)
		notif.embed.text += discordEscape(player) + '\n';
	discordNotif("wwp", notif);
}

static void discordCreateWwpGame(const Game& game)
{
	Notif notif;
	notif.content = "Player **" + discordEscape(game.creator) + "** created a game in channel **" + std::string(game.channel) + "**";
	notif.embed.title = "Players";
	for (auto& [player, channel] : wwpPlayers)
		notif.embed.text += discordEscape(player) + '\n';
	discordNotif("wwp", notif);
}

static void discordJoinWwpGame(const Game& game, const std::string& player)
{
	/* Disabled to avoid too much spam
	Notif notif;
	notif.content = "Player **" + discordEscape(player) + "** has joined " + game.creator + "'s game in channel **" + std::string(game.channel) + "**";
	notif.embed.title = "Players";
	for (const std::string& ingame : game.players)
		notif.embed.text += discordEscape(ingame) + '\n';
	discordNotif("wwp", notif);
	*/
}

static void discordTetrisPlayerJoined(const char *nick)
{
	Notif notif;
	notif.content = "Player **" + discordEscape(nick) + "** has entered the online lobby";
	notif.embed.title = "Players";
	for (const auto& player : tetrisPlayers)
		notif.embed.text += discordEscape(player) + '\n';
	discordNotif("nexttetris", notif);
}

static void discordTetrisChallengeAccepted(const char *nick, const std::string& challenger)
{
	Notif notif;
	notif.content = "Player **" + discordEscape(nick) + "** has accepted "
		"**" + discordEscape(challenger) + "**'s challenge!";
	notif.embed.title = "Players";
	for (const auto& player : tetrisPlayers)
		notif.embed.text += discordEscape(player) + '\n';
	discordNotif("nexttetris", notif);
}

static void discordStarlancerPlayerJoined()
{
	Notif notif;
	notif.content = "A new player has connected";
	notif.embed.title = "Players";
	notif.embed.text = std::to_string(starlancerPlayers.size());
	if (starlancerPlayers.size() >= 2)
		notif.embed.text += " players are online";
	else
		notif.embed.text += " player is online";
	discordNotif("starlancer", notif);
}

static void resetStats()
{
	Lock _(mutex);
	starlancerPlayers.clear();
	tetrisPlayers.clear();
	wwpPlayers.clear();
	wwpGames.clear();
}

static void playerJoined(const char *nick, const char *channel)
{
	try {
		if (!strcmp(STAR_CHAN, channel))
		{
			{
				Lock _(mutex);
				starlancerPlayers.insert(nick);
			}
			discordStarlancerPlayerJoined();
		}
		else if (!strcmp(TET_CHAN, channel))
		{
			{
				Lock _(mutex);
				tetrisPlayers.insert(nick);
			}
			discordTetrisPlayerJoined(nick);
		}
		else
		{
			{
				Lock _(mutex);
				wwpPlayers[nick] = channel;
			}
			discordWwpPlayerJoined(nick, channel + 1);
		}
	} catch (const DiscordException& e) {
		fprintf(stderr, "Discord error: %s", e.what());
	}
}

static void playerParted(const char *nick, const char *channel)
{
	Lock _(mutex);
	if (channel == nullptr || !strcmp(STAR_CHAN, channel))
		starlancerPlayers.erase(nick);
	if (channel == nullptr || !strcmp(TET_CHAN, channel))
		tetrisPlayers.erase(nick);
	wwpPlayers.erase(nick);
}

static void playerMessage(const char *nick, const char *channel, const char *msg)
{
	try {
		if (!strcmp(STAR_CHAN, channel))
			return;
		// Next Tetris
		if (!strcmp(TET_CHAN, channel))
		{
			if (strncmp(msg, "ACCEPT*", 7))
				return;
			msg += 7;
			const char *star = strchr(msg, '*');
			if  (star == nullptr)
				return;
			std::string challenger{ msg, star };
			discordTetrisChallengeAccepted(nick, challenger);
			return;
		}
		// Worms World Party
		if (!strncmp("<r:gamename>", msg, 12))
		{
			// game creation or joining
			const char *bang = strchr(msg, '!');
			if (bang == nullptr)
				return;
			std::string creator{ msg + 12, bang };
			std::string gameId{ bang + 1 };
			if (creator == nick)
			{
				Game game { gameId, creator, channel + 1 };
				game.players.push_back(creator);
				{
					Lock _(mutex);
					wwpGames[gameId] = game;
				}
				discordCreateWwpGame(game);
				//fprintf(stderr, "%s created a game in %s\n", nick, channel);
			}
			else
			{
				auto it = wwpGames.find(gameId);
				if (it != wwpGames.end())
				{
					it->second.players.push_back(nick);
					discordJoinWwpGame(it->second, nick);
					//fprintf(stderr, "%s joined %s's game\n", nick, it->second.creator.c_str());
				}
			}
		}
		else if (!strncmp("<c:", msg, 3))
		{
			// Leaving game
			const char *bang = strchr(msg, '!');
			if (bang == nullptr)
				return;
			const char *end = strchr(bang, '>');
			if (end == nullptr)
				return;
			std::string creator{ msg + 3, bang };
			std::string gameId{ bang + 1, end };
			if (creator == nick)
			{
				Lock _(mutex);
				wwpGames.erase(gameId);
				//fprintf(stderr, "Game %s deleted\n", gameId.c_str());
			}
			else
			{
				auto it = wwpGames.find(gameId);
				if (it == wwpGames.end())
					return;
				auto& players = it->second.players;
				for (auto it2 = players.begin(); it2 != players.end(); ++it2)
				{
					if (*it2 == nick) {
						players.erase(it2);
						//fprintf(stderr, "%s left %s's game\n", nick, it->second.creator.c_str());
						break;
					}
				}
			}
		}
	} catch (const DiscordException& e) {
		fprintf(stderr, "Discord error: %s", e.what());
	}
}

static bool checkChannelTopic(irc_session_t *session, const char *channel, const char *topic)
{
	for (unsigned i = 0; i < std::size(Channels); i++)
	{
		if (!strcmp(Channels[i], channel))
		{
			curTopics[i] = topic;
			if (Topics[i] != nullptr && curTopics[i] != Topics[i])
			{
				if (irc_cmd_topic(session, Channels[i], Topics[i]))
					fprintf(stderr, "TOPIC command failed: %s\n", irc_strerror(irc_errno(session)));
					// TODO
				else
					fprintf(stderr, "Channel %s topic changed to '%s'\n", Channels[i], Topics[i]);
				return true;
			}
			return false;
		}
	}
	fprintf(stderr, "Unknown channel: %s\n", channel);
	return false;
}

void event_connect(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned count)
{
	fprintf(stderr, "Connected to %s\n", origin == nullptr ? "?" : origin);
	for (auto& s : curTopics)
		s.clear();
	irc_cmd_user_mode(session, "+iB");	// invisible, bot
	joinIndex = 1;
	if (irc_cmd_join(session, Channels[0], nullptr))
		fprintf(stderr, "JOIN command failed: %s\n", irc_strerror(irc_errno(session)));
		// TODO?
}

void event_join(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned count)
{
	if (origin == nullptr || count == 0)
		return;
	if (strcmp(BOT_NAME, origin) != 0)
	{
		// Someone else joined
		fprintf(stderr, "User %s joined %s\n", origin, params[0]);
		playerJoined(origin, params[0]);
		return;
	}

	fprintf(stderr, "Joined %s\n", params[0]);
	if (joinIndex < std::size(Channels))
	{
		if (irc_cmd_join(session, Channels[joinIndex++], nullptr)) {
			fprintf(stderr, "JOIN command failed: %s\n", irc_strerror(irc_errno(session)));
			// TODO?
		}
	}
	else
	{
		for (topicIndex = 0; topicIndex < std::size(Channels); topicIndex++)
		{
			if (checkChannelTopic(session, Channels[topicIndex], curTopics[topicIndex].c_str())) {
				topicIndex++;
				break;
			}
		}
	}
}

void event_part(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned count)
{
	if (origin == nullptr || count == 0)
		return;
	//fprintf(stderr, "User %s left %s\n", origin, params[0]);
	playerParted(origin, params[0]);
}

void event_channel(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned count)
{
	if (origin == nullptr || count < 2)
		return;
	const char *msg = params[1];
	//fprintf(stderr, "%s say: %s\n", origin, msg);
	playerMessage(origin, params[0], params[1]);
}

void event_numeric(irc_session_t *session, unsigned event, const char *origin, const char **params, unsigned count)
{
	if (event == 332)
		// Channel topic
		checkChannelTopic(session, params[1], params[2]);
}

void event_topic(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned count)
{
	const char *topic = count >= 2 ? params[1] : "";
	//printf("Event TOPIC from %s: channel %s topic '%s'\n", origin == nullptr ? "?" : origin, params[0], topic);
	checkChannelTopic(session, params[0], topic);
	for (; topicIndex < std::size(Channels); topicIndex++)
	{
		if (checkChannelTopic(session, Channels[topicIndex], curTopics[topicIndex].c_str())) {
			topicIndex++;
			break;
		}
	}
}

void event_quit(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned count)
{
	//fprintf(stderr, "%s has left\n", origin);
	playerParted(origin, nullptr);
}

static bool pingMsxAlpha()
{
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		perror("socket");
		return false;
	}
	sockaddr_in addr {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = 0x0100007f;
	addr.sin_port = htons(28900);
	if (connect(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Connection to msx_alpha failed");
		close(sock);
		return false;
	}
	char buffer[256];
	ssize_t size = read(sock, buffer, sizeof(buffer));
	if (size < 0) {
		perror("Receive from msx_alpha failed");
	}
	else if (size == 0) {
		fprintf(stderr, "msx_alpha closed the connexion\n");
	}
	bool status = false;
	if (size > 0) {
		if (!strncmp(buffer, "\\basic\\\\secure\\", 15))
			status = true;
	}
	shutdown(sock, SHUT_WR);
	close(sock);
	return status;
}

static void statusUpdater()
{
	for (;;)
	{
		int wwpPlayers, wwpGames;
		int tetrisPlayers;
		int starlancerPlayers;
		{
			Lock _(mutex);
			wwpPlayers = ::wwpPlayers.size();
			wwpGames = ::wwpGames.size();
			tetrisPlayers = ::tetrisPlayers.size();
			starlancerPlayers = ::starlancerPlayers.size();
		}
		statusUpdate("wwp", wwpPlayers, wwpGames);
		statusUpdate("nexttetris", tetrisPlayers, 0);
		try {
			statusCommit("ircbot");
		} catch (const std::exception& e) {
			fprintf(stderr, "statusCommit(ircbot) failed: %s\n", e.what());
		}
		if (pingMsxAlpha()) {
			statusUpdate("starlancer", starlancerPlayers, 0);
			try {
				statusCommit("starlancer");
			} catch (const std::exception& e) {
				fprintf(stderr, "statusCommit(starlancer) failed: %s\n", e.what());
			}
		}

		sleep((unsigned)statusGetInterval());
	}
}

int main(int argc, char *argv[])
{
	// The IRC callbacks structure
	irc_callbacks_t callbacks {};

	// Set up the mandatory events
	callbacks.event_connect = event_connect;
	callbacks.event_numeric = event_numeric;

	// Set up the rest of events
	callbacks.event_join = event_join;
	callbacks.event_topic = event_topic;
	callbacks.event_part = event_part;
	callbacks.event_channel = event_channel;
	callbacks.event_quit = event_quit;

	std::thread statusThread(statusUpdater);
	statusThread.detach();

	while (true)
	{
		// Now create the session
		irc_session_t *session = irc_create_session(&callbacks);

		if (session == nullptr) {
			fprintf(stderr, "Can't create irc session\n");
			return 1;
		}
		irc_option_set(session, LIBIRC_OPTION_STRIPNICKS);

		if (irc_connect(session, argc >= 2 ? argv[1] : "localhost", 6667, 0, BOT_NAME, BOT_NAME, "DCNet bot"))
		{
			fprintf(stderr, "Can't connect to the IRC server: %s\n", irc_strerror(irc_errno(session)));
			irc_destroy_session(session);
			sleep(30);
			continue;
		}
		if (irc_run(session)) {
			// Either the connection to the server could not be established or was terminated. See irc_errno()
			fprintf(stderr, "Connection terminated: %s\n", irc_strerror(irc_errno(session)));
			sleep(30);
		}
		irc_destroy_session(session);
		resetStats();
	}

	return 0;
}

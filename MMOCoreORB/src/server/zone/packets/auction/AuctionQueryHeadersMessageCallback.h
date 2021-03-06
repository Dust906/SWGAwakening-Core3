/*
 * AuctionQueryHeadersMessageCallback.h
 *
 *  Created on: 30/01/2010
 *      Author: victor
 */

#ifndef AUCTIONQUERYHEADERSMESSAGECALLBACK_H_
#define AUCTIONQUERYHEADERSMESSAGECALLBACK_H_

#include "server/zone/packets/MessageCallback.h"
#include "server/zone/managers/auction/AuctionManager.h"

class AuctionQueryHeadersMessageCallback : public MessageCallback {
	int extent;
	int counter;
	int screen;
	uint32 category;
	int itemType;
	UnicodeString filterText;
	int unk1;
	int minPrice;
	int maxPrice;
	bool includeEntranceFee;
	uint64 vendorID;
	bool isVendor;
	int offset;

public:
	AuctionQueryHeadersMessageCallback(ZoneClientSession* client, ZoneProcessServer* server) :
			MessageCallback(client, server), extent(0), counter(0), screen(0), category(0), itemType(0), unk1(0), minPrice(0), maxPrice(0), includeEntranceFee(0), vendorID(0), isVendor(0), offset(0) {

	}

	void parse(Message* message) {
		extent = message->parseInt();
		// 0 - galaxy, 1 - planet, 2 - region, 3 - vendor
		counter = message->parseInt();
		screen = message->parseInt();
		// 2 - all items, 3 - my sales, 4 - my bids, 5 - available items,
		// 7 - for sale (vendor), 9 - offers to vendor
		category = message->parseInt();  // Bitmask

		itemType = message->parseInt();
		message->parseUnicode(filterText);
		unk1 = message->parseInt();
		minPrice = message->parseInt();
		maxPrice = message->parseInt();
		includeEntranceFee = message->parseByte();

		vendorID = message->parseLong();
		isVendor = message->parseByte();
		offset = message->parseShort();

	}

	void run() {
		ManagedReference<CreatureObject*> player = client->getPlayer();

		if (player == NULL)
			return;
			
		Locker locker(player);

		AuctionManager* auctionManager = server->getZoneServer()->getAuctionManager();

		if (auctionManager != NULL)
			auctionManager->getData(player, extent, vendorID, screen, category, filterText, minPrice, maxPrice, includeEntranceFee, counter, offset);
	}
};

#endif /* AUCTIONQUERYHEADERSMESSAGECALLBACK_H_ */

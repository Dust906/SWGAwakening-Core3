#ifndef FRSRANKINGDATA_H_
#define FRSRANKINGDATA_H_

namespace server {
namespace zone {
namespace managers {
namespace frs {

class FrsRankingData : public Object {

protected:
	int rank, requiredXp, playerCap;
	String skillName;
	String titleName;

public:
	FrsRankingData(int frsRank, String skill, String title, int reqExp, int cap) : Object() {
		rank = frsRank;
		skillName = skill;
		requiredXp = reqExp;
		playerCap = cap;
		titleName = title;
	}

	~FrsRankingData() {
	}

	int getRequiredExperience() const {
		return requiredXp;
	}

	int getPlayerCap() const {
		return playerCap;
	}

	int getRank() const {
		return rank;
	}

	const String& getSkillName() const {
		return skillName;
	}

	const String& getTitleName() const {
		return titleName;
	}

};

}
}
}
}

#endif /* FRSRANKINGDATA_H_ */

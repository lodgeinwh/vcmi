#include "StdInc.h"
#include "Goals.h"
#include "VCAI.h"
#include "Fuzzy.h"

/*
 * Goals.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

extern boost::thread_specific_ptr<CCallback> cb;
extern boost::thread_specific_ptr<VCAI> ai;
extern FuzzyHelper * fh; //TODO: this logic should be moved inside VCAI

using namespace vstd;
using namespace Goals;

TSubgoal Goals::sptr(const AbstractGoal & tmp)
{
	shared_ptr<AbstractGoal> ptr;
	ptr.reset(tmp.clone());
	return ptr;
}

std::string Goals::AbstractGoal::name() const //TODO: virtualize
{
	switch (goalType)
	{
		case INVALID:
			return "INVALID";
		case WIN:
			return "WIN";
		case DO_NOT_LOSE:
			return "DO NOT LOOSE";
		case CONQUER:
			return "CONQUER";
		case BUILD:
			return "BUILD";
		case EXPLORE:
			return "EXPLORE";
		case GATHER_ARMY:
			return "GATHER ARMY";
		case BOOST_HERO:
			return "BOOST_HERO (unsupported)";
		case RECRUIT_HERO:
			return "RECRUIT HERO";
		case BUILD_STRUCTURE:
			return "BUILD STRUCTURE";
		case COLLECT_RES:
			return "COLLECT RESOURCE";
		case GATHER_TROOPS:
			return "GATHER TROOPS";
		case GET_OBJ:
			return "GET OBJECT " + boost::lexical_cast<std::string>(objid);
		case FIND_OBJ:
			return "FIND OBJECT " + boost::lexical_cast<std::string>(objid);
		case VISIT_HERO:
			return "VISIT HERO " + boost::lexical_cast<std::string>(objid);
		case GET_ART_TYPE:
			return "GET ARTIFACT OF TYPE " + VLC->arth->artifacts[aid]->Name();
		case ISSUE_COMMAND:
			return "ISSUE COMMAND (unsupported)";
		case VISIT_TILE:
			return "VISIT TILE " + tile();
		case CLEAR_WAY_TO:
			return "CLEAR WAY TO " + tile();
		case DIG_AT_TILE:
			return "DIG AT TILE " + tile();
		default:
			return boost::lexical_cast<std::string>(goalType);
	}
}

//TODO: find out why the following are not generated automatically on MVS?

namespace Goals 
{ 
	template <>
	void CGoal<Win>::accept (VCAI * ai)
	{
		ai->tryRealize(static_cast<Win&>(*this));
	}

	template <>
	void CGoal<Build>::accept (VCAI * ai)
	{
		ai->tryRealize(static_cast<Build&>(*this));
	}
	template <>
	float CGoal<Win>::accept (FuzzyHelper * f)
	{
		return f->evaluate(static_cast<Win&>(*this));
	}

	template <>
	float CGoal<Build>::accept (FuzzyHelper * f)
	{
		return f->evaluate(static_cast<Build&>(*this));
	}
}

//TSubgoal AbstractGoal::whatToDoToAchieve()
//{
//    logAi->debugStream() << boost::format("Decomposing goal of type %s") % name();
//        return sptr (Goals::Explore());
//}

TSubgoal Win::whatToDoToAchieve()
{
	const VictoryCondition &vc = cb->getMapHeader()->victoryCondition;
	EVictoryConditionType::EVictoryConditionType cond = vc.condition;

	if(!vc.appliesToAI)
	{
		//TODO deduce victory from human loss condition
		cond = EVictoryConditionType::WINSTANDARD;
	}

	switch(cond)
	{
	case EVictoryConditionType::ARTIFACT:
		return sptr (Goals::GetArtOfType(vc.objectId));
	case EVictoryConditionType::BEATHERO:
		return sptr (Goals::GetObj(vc.obj->id.getNum()));
	case EVictoryConditionType::BEATMONSTER:
		return sptr (Goals::GetObj(vc.obj->id.getNum()));
	case EVictoryConditionType::BUILDCITY:
		//TODO build castle/capitol
		break;
	case EVictoryConditionType::BUILDGRAIL:
		{
			if(auto h = ai->getHeroWithGrail())
			{
				//hero is in a town that can host Grail
				if(h->visitedTown && !vstd::contains(h->visitedTown->forbiddenBuildings, BuildingID::GRAIL))
				{
					const CGTownInstance *t = h->visitedTown;
					return sptr (Goals::BuildThis(BuildingID::GRAIL, t));
				}
				else
				{
					auto towns = cb->getTownsInfo();
					towns.erase(boost::remove_if(towns,
										[](const CGTownInstance *t) -> bool
										{
											return vstd::contains(t->forbiddenBuildings, BuildingID::GRAIL);
										}),
								towns.end());
					boost::sort(towns, isCloser);
					if(towns.size())
					{
						return sptr (Goals::VisitTile(towns.front()->visitablePos()).sethero(h));
					}
				}
			}
			double ratio = 0;
			int3 grailPos = cb->getGrailPos(ratio);
			if(ratio > 0.99)
			{
				return sptr (Goals::DigAtTile(grailPos));
			} //TODO: use FIND_OBJ
			else if(const CGObjectInstance * obj = ai->getUnvisitedObj(objWithID<Obj::OBELISK>)) //there are unvisited Obelisks
			{
				return sptr (Goals::GetObj(obj->id.getNum()));
			}
			else
				return sptr (Goals::Explore());
		}
		break;
	case EVictoryConditionType::CAPTURECITY:
		return sptr (Goals::GetObj(vc.obj->id.getNum()));
	case EVictoryConditionType::GATHERRESOURCE:
        return sptr (Goals::CollectRes(static_cast<Res::ERes>(vc.objectId), vc.count));
		//TODO mines? piles? marketplace?
		//save?
		break;
	case EVictoryConditionType::GATHERTROOP:
		return sptr (Goals::GatherTroops(vc.objectId, vc.count));
		break;
	case EVictoryConditionType::TAKEDWELLINGS:
		break;
	case EVictoryConditionType::TAKEMINES:
		break;
	case EVictoryConditionType::TRANSPORTITEM:
		break;
	case EVictoryConditionType::WINSTANDARD:
		return sptr (Goals::Conquer());
	default:
		assert(0);
	}
	return sptr (Goals::Invalid());
}

TSubgoal FindObj::whatToDoToAchieve()
{
	const CGObjectInstance * o = nullptr;
	if (resID > -1) //specified
	{
		for(const CGObjectInstance *obj : ai->visitableObjs)
		{
			if(obj->ID == objid && obj->subID == resID)
			{
				o = obj;
				break; //TODO: consider multiple objects and choose best
			}
		}
	}
	else
	{
		for(const CGObjectInstance *obj : ai->visitableObjs)
		{
			if(obj->ID == objid)
			{
				o = obj;
				break; //TODO: consider multiple objects and choose best
			}
		}
	}
	if (o && isReachable(o)) //we don't use isAccessibleForHero as we don't know which hero it is
		return sptr (Goals::GetObj(o->id.getNum()));
	else
		return sptr (Goals::Explore());
}
float FindObj::importanceWhenLocked() const
{
	return 1; //we will probably fins it anyway, someday
}

std::string GetObj::completeMessage() const
{
	return "hero " + hero.get()->name + " captured Object ID = " + boost::lexical_cast<std::string>(objid); 
}

TSubgoal GetObj::whatToDoToAchieve()
{
	const CGObjectInstance * obj = cb->getObj(ObjectInstanceID(objid));
	if(!obj)
		return sptr (Goals::Explore());
	int3 pos = obj->visitablePos();
	return sptr (Goals::VisitTile(pos));
}

float GetObj::importanceWhenLocked() const
{
	return 3;
}


bool GetObj::fulfillsMe (shared_ptr<VisitTile> goal)
{
	if (cb->getObj(ObjectInstanceID(objid))->visitablePos() == goal->tile)
		return true;
	else
		return false;
}

std::string VisitHero::completeMessage() const
{
	return "hero " + hero.get()->name + " visited hero " + boost::lexical_cast<std::string>(objid); 
}

TSubgoal VisitHero::whatToDoToAchieve()
{
	const CGObjectInstance * obj = cb->getObj(ObjectInstanceID(objid));
	if(!obj)
		return sptr (Goals::Explore());
	int3 pos = obj->visitablePos();

	if (hero && ai->isAccessibleForHero(pos, hero, true) && isSafeToVisit(hero, pos)) //enemy heroes can get reinforcements
	{
		assert (hero->pos != pos); //don't try to visit yourself
		settile(pos).setisElementar(true);
		return sptr (*this);
	}
	return sptr (Goals::Invalid());
}

float VisitHero::importanceWhenLocked() const
{
	return 4;
}

bool VisitHero::fulfillsMe (shared_ptr<VisitTile> goal)
{
	if (cb->getObj(ObjectInstanceID(objid))->visitablePos() == goal->tile)
		return true;
	else
		return false;
}

TSubgoal GetArtOfType::whatToDoToAchieve()
{
	TSubgoal alternativeWay = CGoal::lookForArtSmart(aid); //TODO: use
	if(alternativeWay->invalid())
		return sptr (Goals::FindObj(Obj::ARTIFACT, aid));
	return sptr (Goals::Invalid());
}

float GetArtOfType::importanceWhenLocked() const
{
	return 2;
}

TSubgoal ClearWayTo::whatToDoToAchieve()
{
	assert(tile.x >= 0); //set tile
	if(!cb->isVisible(tile))
	{
        logAi->errorStream() << "Clear way should be used with visible tiles!";
		return sptr (Goals::Explore());
	}

	HeroPtr h = hero ? hero : ai->primaryHero();
	if(!h)
		return sptr (Goals::RecruitHero());

	cb->setSelection(*h);

	SectorMap sm;
	bool dropToFile = false;
	if(dropToFile) //for debug purposes
		sm.write("test.txt");

	int3 tileToHit = sm.firstTileToGet(h, tile);
	//if(isSafeToVisit(h, tileToHit))
	if(isBlockedBorderGate(tileToHit))
	{	//FIXME: this way we'll not visit gate and activate quest :?
		return sptr (Goals::FindObj (Obj::KEYMASTER, cb->getTile(tileToHit)->visitableObjects.back()->subID));
	}

	//FIXME: this code shouldn't be necessary
	if(tileToHit == tile)
	{
        logAi->errorStream() << boost::format("Very strange, tile to hit is %s and tile is also %s, while hero %s is at %s\n")
			% tileToHit % tile % h->name % h->visitablePos();
		throw cannotFulfillGoalException("Retrieving first tile to hit failed (probably)!");
	}

	auto topObj = backOrNull(cb->getVisitableObjs(tileToHit));
	if(topObj && topObj->ID == Obj::HERO && cb->getPlayerRelations(h->tempOwner, topObj->tempOwner) != PlayerRelations::ENEMIES)
	{
		std::string problem = boost::str(boost::format("%s stands in the way of %s.\n") % topObj->getHoverText()  % h->getHoverText());
		throw cannotFulfillGoalException(problem);
	}

	return sptr (Goals::VisitTile(tileToHit).sethero(h));
	//FIXME:: attempts to visit completely unreachable tile with hero results in stall

	//TODO czy istnieje lepsza droga?

	throw cannotFulfillGoalException("Cannot reach given tile!"); //how and when could this be used?
}

float ClearWayTo::importanceWhenLocked() const
{
	return 5;
}

std::string Explore::completeMessage() const
{
	return "Hero " + hero.get()->name + " completed exploration";
};

TSubgoal Explore::whatToDoToAchieve()
{
	auto objs = ai->visitableObjs; //try to use buildings that uncover map
	erase_if(objs, [&](const CGObjectInstance *obj) -> bool
	{
		if (vstd::contains(ai->alreadyVisited, obj))
			return true;
		switch (obj->ID.num)
		{
			case Obj::REDWOOD_OBSERVATORY:
			case Obj::PILLAR_OF_FIRE:
			case Obj::CARTOGRAPHER:
			case Obj::SUBTERRANEAN_GATE: //TODO: check ai->knownSubterraneanGates
			//case Obj::MONOLITH1:
			//case obj::MONOLITH2:
			//case obj::MONOLITH3:
			//case Obj::WHIRLPOOL:
				return false; //do not erase
				break;
			default:
				return true;
		}
	});
	if (objs.size())
	{
		if (hero.get(true))
		{
			for (auto obj : objs)
			{
				auto pos = obj->visitablePos();
				//FIXME: this confition fails if everything but guarded subterranen gate was explored. in this case we should gather army for hero
				if (isSafeToVisit(hero, pos) && ai->isAccessibleForHero(pos, hero))
					return sptr (Goals::VisitTile(pos).sethero(hero));
			}
		}
		else
		{
			for (auto obj : objs)
			{
				auto pos = obj->visitablePos();
				if (ai->isAccessible (pos)) //TODO: check safety?
					return sptr (Goals::VisitTile(pos).sethero(hero));
			}
		}
	}

	if (hero)
	{
		int3 t = whereToExplore(hero);
		if (t.z == -1) //no safe tile to explore - we need to break!
		{
			erase_if (objs, [&](const CGObjectInstance *obj) -> bool
			{
				switch (obj->ID.num)
				{
					case Obj::CARTOGRAPHER:
					case Obj::SUBTERRANEAN_GATE:
					//case Obj::MONOLITH1:
					//case obj::MONOLITH2:
					//case obj::MONOLITH3:
					//case Obj::WHIRLPOOL:
						return false; //do not erase
						break;
					default:
						return true;
				}
			});
			if (objs.size())
			{
				return sptr (Goals::VisitTile(objs.front()->visitablePos()).sethero(hero).setisAbstract(true));
			}
			else
				throw cannotFulfillGoalException("Cannot explore - no possible ways found!");
		}
		return sptr (Goals::VisitTile(t).sethero(hero));
	}

	auto hs = cb->getHeroesInfo();
	int howManyHeroes = hs.size();

	erase(hs, [](const CGHeroInstance *h)
	{
		return contains(ai->lockedHeroes, h);
	});
	if(hs.empty()) //all heroes are busy. buy new one
	{
		if (howManyHeroes < 3  && ai->findTownWithTavern()) //we may want to recruit second hero. TODO: make it smart finally
			return sptr (Goals::RecruitHero());
		else //find mobile hero with weakest army
		{
			hs = cb->getHeroesInfo();
			erase_if(hs, [](const CGHeroInstance *h)
			{
				return !h->movement; //only hero with movement are of interest for us
			});
			if (hs.empty())
			{
				if (howManyHeroes < GameConstants::MAX_HEROES_PER_PLAYER)
					return sptr (Goals::RecruitHero());
				else
					throw cannotFulfillGoalException("No heroes with remaining MPs for exploring!\n");
			}
			boost::sort(hs, compareMovement); //closer to what?
		}
	}

	const CGHeroInstance *h = hs.front();

	return sptr (sethero(h).setisAbstract(true));

	return iAmElementar(); //FIXME: how can this be called?
};

float Explore::importanceWhenLocked() const
{
	return 1; //exploration is natural and lowpriority process
}

TSubgoal RecruitHero::whatToDoToAchieve()
{
	const CGTownInstance *t = ai->findTownWithTavern();
	if(!t)
		return sptr (Goals::BuildThis(BuildingID::TAVERN));

	if(cb->getResourceAmount(Res::GOLD) < HERO_GOLD_COST)
		return sptr (Goals::CollectRes(Res::GOLD, HERO_GOLD_COST));

	return iAmElementar();
}

std::string VisitTile::completeMessage() const
{
	return "Hero " + hero.get()->name + " visited tile " + tile();
}

TSubgoal VisitTile::whatToDoToAchieve()
{
	//here only temporarily
	auto ret = fh->chooseSolution (getAllPossibleSubgoals());

	if (ret->hero)
	{
		if (isSafeToVisit(ret->hero, tile))
		{
			ret->setisElementar(true);
			return ret;
		}
		else
		{
			return sptr (Goals::GatherArmy(evaluateDanger(tile, *ret->hero) * SAFE_ATTACK_CONSTANT).sethero(ret->hero));
		}
	}
	return ret;
	//if(!cb->isVisible(tile))
	//	return sptr (Goals::Explore());

	//if(hero && !ai->isAccessibleForHero(tile, hero))
	//	hero = nullptr;

	//if(!hero)
	//{
	//	if(cb->getHeroesInfo().empty())
	//	{
	//		return sptr (Goals::RecruitHero());
	//	}

	//	for(const CGHeroInstance *h : cb->getHeroesInfo())
	//	{
	//		if(ai->isAccessibleForHero(tile, h))
	//		{
	//			hero = h;
	//			break;
	//		}
	//	}
	//}

	//if(hero)
	//{
	//	if(isSafeToVisit(hero, tile))
	//		return sptr (setisElementar(true));
	//	else
	//	{
	//		return sptr (Goals::GatherArmy(evaluateDanger(tile, *hero) * SAFE_ATTACK_CONSTANT).sethero(hero));
	//	}
	//}
	//else	//inaccessible for all heroes
	//{
	//	return sptr (Goals::ClearWayTo(tile));
	//}
}

float VisitTile::importanceWhenLocked() const
{
	return 5; //depends on a distance, but we should really reach the tile once it was selected
}

TGoalVec VisitTile::getAllPossibleSubgoals()
{
	TGoalVec ret;
	if (!cb->isVisible(tile))
		ret.push_back (sptr(Goals::Explore())); //what sense does it make?
	else
	{
		for (auto h : cb->getHeroesInfo())
		{
			if (ai->isAccessibleForHero(tile, h))
				ret.push_back (sptr(Goals::VisitTile(tile).sethero(h)));
		}
		if (ai->canRecruitAnyHero())
			ret.push_back (sptr(Goals::RecruitHero()));
	}
	if (ret.empty())
		ret.push_back (sptr(Goals::ClearWayTo(tile)));
	//important - at least one sub-goal must handle case which is impossible to fulfill (unreachable tile)
	return ret;
}

TSubgoal DigAtTile::whatToDoToAchieve()
{
	const CGObjectInstance *firstObj = frontOrNull(cb->getVisitableObjs(tile));
	if(firstObj && firstObj->ID == Obj::HERO && firstObj->tempOwner == ai->playerID) //we have hero at dest
	{
		const CGHeroInstance *h = dynamic_cast<const CGHeroInstance *>(firstObj);
		sethero(h).setisElementar(true);
		return sptr (*this);
	}

	return sptr (Goals::VisitTile(tile));
}

float DigAtTile::importanceWhenLocked() const
{
	return 20; //do not! interrupt tile digging
}

TSubgoal BuildThis::whatToDoToAchieve()
{
	//TODO check res
	//look for town
	//prerequisites?
	return iAmElementar();
}

float BuildThis::importanceWhenLocked() const
{
	return 5;
}

TSubgoal CollectRes::whatToDoToAchieve()
{
	std::vector<const IMarket*> markets;

	std::vector<const CGObjectInstance*> visObjs;
	ai->retreiveVisitableObjs(visObjs, true);
	for(const CGObjectInstance *obj : visObjs)
	{
		if(const IMarket *m = IMarket::castFrom(obj, false))
		{
			if(obj->ID == Obj::TOWN && obj->tempOwner == ai->playerID && m->allowsTrade(EMarketMode::RESOURCE_RESOURCE))
				markets.push_back(m);
			else if(obj->ID == Obj::TRADING_POST) //TODO a moze po prostu test na pozwalanie handlu?
				markets.push_back(m);
		}
	}

	boost::sort(markets, [](const IMarket *m1, const IMarket *m2) -> bool
	{
		return m1->getMarketEfficiency() < m2->getMarketEfficiency();
	});

	markets.erase(boost::remove_if(markets, [](const IMarket *market) -> bool
	{
		return !(market->o->ID == Obj::TOWN && market->o->tempOwner == ai->playerID)
			&& !ai->isAccessible(market->o->visitablePos());
	}),markets.end());

	if(!markets.size())
	{
		for(const CGTownInstance *t : cb->getTownsInfo())
		{
			if(cb->canBuildStructure(t, BuildingID::MARKETPLACE) == EBuildingState::ALLOWED)
				return sptr (Goals::BuildThis(BuildingID::MARKETPLACE, t));
		}
	}
	else
	{
		const IMarket *m = markets.back();
		//attempt trade at back (best prices)
		int howManyCanWeBuy = 0;
		for(Res::ERes i = Res::WOOD; i <= Res::GOLD; vstd::advance(i, 1))
		{
			if(i == resID) continue;
			int toGive = -1, toReceive = -1;
			m->getOffer(i, resID, toGive, toReceive, EMarketMode::RESOURCE_RESOURCE);
			assert(toGive > 0 && toReceive > 0);
			howManyCanWeBuy += toReceive * (cb->getResourceAmount(i) / toGive);
		}

		if(howManyCanWeBuy + cb->getResourceAmount(static_cast<Res::ERes>(resID)) >= value)
		{
			auto backObj = backOrNull(cb->getVisitableObjs(m->o->visitablePos())); //it'll be a hero if we have one there; otherwise marketplace
			assert(backObj);
			if (backObj->tempOwner != ai->playerID)
			{
				return sptr (Goals::GetObj(m->o->id.getNum()));
			}
			else
			{
				return sptr (Goals::GetObj(m->o->id.getNum()).setisElementar(true));
			}
		}
	}
	return sptr (Goals::Invalid()); //FIXME: unused?
}

float CollectRes::importanceWhenLocked() const
{
	return 2;
}

TSubgoal GatherTroops::whatToDoToAchieve()
{
	std::vector<const CGDwelling *> dwellings;
	for(const CGTownInstance *t : cb->getTownsInfo())
	{
		auto creature = VLC->creh->creatures[objid];
		if (t->subID == creature->faction) //TODO: how to force AI to build unupgraded creatures? :O
		{
			auto creatures = vstd::tryAt(t->town->creatures, creature->level - 1);
			if(!creatures)
				continue; 

			int upgradeNumber = vstd::find_pos(*creatures, creature->idNumber);
			if(upgradeNumber < 0)
				continue;

			BuildingID bid(BuildingID::DWELL_FIRST + creature->level - 1 + upgradeNumber * GameConstants::CREATURES_PER_TOWN);
			if (t->hasBuilt(bid)) //this assumes only creatures with dwellings are assigned to faction
			{
				dwellings.push_back(t);
			}
			else
			{
				return sptr (Goals::BuildThis(bid, t));
			}
		}
	}
	for (auto obj : ai->visitableObjs)
	{
		if (obj->ID != Obj::CREATURE_GENERATOR1) //TODO: what with other creature generators?
			continue;

		auto d = dynamic_cast<const CGDwelling *>(obj);
		for (auto creature : d->creatures)
		{
			if (creature.first) //there are more than 0 creatures avaliabe
			{
				for (auto type : creature.second)
				{
					if (type == objid  &&  ai->freeResources().canAfford(VLC->creh->creatures[type]->cost))
						dwellings.push_back(d);
				}
			}
		}
	}
	if (dwellings.size())
	{
		boost::sort(dwellings, isCloser);
		return sptr (Goals::GetObj(dwellings.front()->id.getNum()));
	}
	else
		return sptr (Goals::Explore());
	//TODO: exchange troops between heroes
}

float GatherTroops::importanceWhenLocked() const
{
	return 2;
}

TSubgoal Conquer::whatToDoToAchieve()
{
	auto hs = cb->getHeroesInfo();
	int howManyHeroes = hs.size();

	erase(hs, [](const CGHeroInstance *h)
	{
		return contains(ai->lockedHeroes, h);
	});
	if(hs.empty()) //all heroes are busy. buy new one
	{
		if (howManyHeroes < 3  && ai->findTownWithTavern()) //we may want to recruit second hero. TODO: make it smart finally
			return sptr (Goals::RecruitHero());
		else //find mobile hero with weakest army
		{
			hs = cb->getHeroesInfo();
			erase_if(hs, [](const CGHeroInstance *h)
			{
				return !h->movement; //only hero with movement are of interest for us
			});
			if (hs.empty())
			{
				if (howManyHeroes < GameConstants::MAX_HEROES_PER_PLAYER)
					return sptr (Goals::RecruitHero());
				else
					throw cannotFulfillGoalException("No heroes with remaining MPs for exploring!\n");
			}
			boost::sort(hs, compareHeroStrength);
		}
	}

	const CGHeroInstance *h = hs.back();
	cb->setSelection(h);
	std::vector<const CGObjectInstance *> objs; //here we'll gather enemy towns and heroes
	ai->retreiveVisitableObjs(objs);
	erase_if(objs, [&](const CGObjectInstance *obj)
	{
		return (obj->ID != Obj::TOWN && obj->ID != Obj::HERO) //not town/hero
			|| cb->getPlayerRelations(ai->playerID, obj->tempOwner) != PlayerRelations::ENEMIES;
	});
			
	if (objs.empty()) //experiment - try to conquer dwellings and mines, it should pay off
	{
		ai->retreiveVisitableObjs(objs);
		erase_if(objs, [&](const CGObjectInstance *obj)
		{
			return (obj->ID != Obj::CREATURE_GENERATOR1 && obj->ID != Obj::MINE) //not dwelling or mine
				|| cb->getPlayerRelations(ai->playerID, obj->tempOwner) != PlayerRelations::ENEMIES;
		});
	}

	if(objs.empty())
		return sptr (Goals::Explore()); //we need to find an enemy

	erase_if(objs,  [&](const CGObjectInstance *obj)
	{
		return !isSafeToVisit(h, obj->visitablePos()) || vstd::contains (ai->reservedObjs, obj); //no need to capture same object twice
	});

	if(objs.empty())
		return iAmElementar();

	boost::sort(objs, isCloser);
	for(const CGObjectInstance *obj : objs)
	{
		if (ai->isAccessibleForHero(obj->visitablePos(), h))
		{
			ai->reserveObject(h, obj); //no one else will capture same object until we fail

			if (obj->ID == Obj::HERO)
				return sptr (Goals::VisitHero(obj->id.getNum()).sethero(h).setisAbstract(true));
					//track enemy hero
			else
				return sptr (Goals::VisitTile(obj->visitablePos()).sethero(h));
		}
	}

	return sptr (Goals::Explore()); //enemy is inaccessible
}
float Conquer::importanceWhenLocked() const
{
	return 10; //defeating opponent is hig priority, always
}


TSubgoal Build::whatToDoToAchieve()
{
	return iAmElementar();
}

float Build::importanceWhenLocked() const
{
	return 1;
}

TSubgoal Invalid::whatToDoToAchieve()
{
	return iAmElementar();
}

std::string GatherArmy::completeMessage() const
{
	return "Hero " + hero.get()->name + " gathered army of value " + boost::lexical_cast<std::string>(value);
};

TSubgoal GatherArmy::whatToDoToAchieve()
{
	//TODO: find hero if none set
	assert(hero);

	cb->setSelection(*hero);
	auto compareReinforcements = [this](const CGTownInstance *lhs, const CGTownInstance *rhs) -> bool
	{
		return howManyReinforcementsCanGet(hero, lhs) < howManyReinforcementsCanGet(hero, rhs);
	};

	std::vector<const CGTownInstance *> townsReachable;
	for(const CGTownInstance *t : cb->getTownsInfo())
	{
		if(!t->visitingHero && howManyReinforcementsCanGet(hero,t))
		{
			if (ai->isAccessibleForHero(t->pos, hero) && !vstd::contains (ai->townVisitsThisWeek[hero], t))
				townsReachable.push_back(t);
		}
	}

	if(townsReachable.size()) //try towns first
	{
		boost::sort(townsReachable, compareReinforcements);
		return sptr (Goals::VisitTile(townsReachable.back()->visitablePos()).sethero(hero));
	}
	else
	{
		if (hero == ai->primaryHero()) //we can get army from other heroes
		{
			auto otherHeroes = cb->getHeroesInfo();
			auto heroDummy = hero;
			erase_if(otherHeroes, [heroDummy](const CGHeroInstance * h)
			{
				return (h == heroDummy.h || !ai->isAccessibleForHero(heroDummy->visitablePos(), h, true) || !ai->canGetArmy(heroDummy.h, h));
			});
			if (otherHeroes.size())
			{
				boost::sort(otherHeroes, compareArmyStrength); //TODO:  check if hero has at least one stack more powerful than ours? not likely to fail
				int primaryPath, secondaryPath;
				auto h = otherHeroes.back();
				cb->setSelection(hero.h);
				primaryPath = cb->getPathInfo(h->visitablePos())->turns;
				cb->setSelection(h);
				secondaryPath = cb->getPathInfo(hero->visitablePos())->turns;

				if (primaryPath < secondaryPath)
					return sptr (Goals::VisitHero(h->id.getNum()).setisAbstract(true).sethero(hero));
						//go to the other hero if we are faster
				else
					return sptr (Goals::VisitHero(hero->id.getNum()).setisAbstract(true).sethero(h));
						//let the other hero come to us
			}
		}

		std::vector<const CGObjectInstance *> objs; //here we'll gather all dwellings
		ai->retreiveVisitableObjs(objs, true);
		erase_if(objs, [&](const CGObjectInstance *obj)
		{
			if(obj->ID != Obj::CREATURE_GENERATOR1)
				return true;

			auto relationToOwner = cb->getPlayerRelations(obj->getOwner(), ai->playerID);
			if(relationToOwner == PlayerRelations::ALLIES)
				return true;

			//Use flagged dwellings only when there are available creatures that we can afford
			if(relationToOwner == PlayerRelations::SAME_PLAYER)
			{
				auto dwelling = dynamic_cast<const CGDwelling*>(obj);
				for(auto & creLevel : dwelling->creatures)
				{
					if(creLevel.first)
					{
						for(auto & creatureID : creLevel.second)
						{
							auto creature = VLC->creh->creatures[creatureID];
							if(ai->freeResources().canAfford(creature->cost))
								return false;
						}
					}
				}
			}

			return true;
		});
		if(objs.empty()) //no possible objects, we did eveyrthing already
			return sptr (Goals::Explore(hero));
		//TODO: check if we can recruit any creatures there, evaluate army
		else
		{
			boost::sort(objs, isCloser);
			HeroPtr h = nullptr;
			for(const CGObjectInstance *obj : objs)
			{ //find safe dwelling
				auto pos = obj->visitablePos();
				if (shouldVisit (hero, obj)) //creatures fit in army
					h = hero;
				else
				{
					for(auto ourHero : cb->getHeroesInfo()) //make use of multiple heroes
					{
						if (shouldVisit(ourHero, obj))
							h = ourHero;
					}
				}
				if (h && isSafeToVisit(h, pos) && ai->isAccessibleForHero(pos, h))
					return sptr (Goals::VisitTile(pos).sethero(h));
			}
		}
	}

	return sptr (Goals::Explore(hero)); //find dwelling. use current hero to prevent him from doing nothing.
}

float GatherArmy::importanceWhenLocked() const
{
	return 2.5;
}

//TSubgoal AbstractGoal::whatToDoToAchieve()
//{
//    logAi->debugStream() << boost::format("Decomposing goal of type %s") % name();
//	return sptr (Goals::Explore());
//}

TSubgoal AbstractGoal::goVisitOrLookFor(const CGObjectInstance *obj)
{
	if(obj)
		return sptr (Goals::GetObj(obj->id.getNum()));
	else
		return sptr (Goals::Explore());
}

TSubgoal AbstractGoal::lookForArtSmart(int aid)
{
	return sptr (Goals::Invalid());
}

bool AbstractGoal::invalid() const
{
	return goalType == INVALID;
}

void AbstractGoal::accept (VCAI * ai)
{
	ai->tryRealize(*this);
}


template<typename T>
void CGoal<T>::accept (VCAI * ai)
{
	ai->tryRealize(static_cast<T&>(*this)); //casting enforces template instantiation
}

float AbstractGoal::accept (FuzzyHelper * f)
{
	return f->evaluate(*this);
}

template<typename T>
float  CGoal<T>::accept (FuzzyHelper * f)
{
	return f->evaluate(static_cast<T&>(*this)); //casting enforces template instantiation
}


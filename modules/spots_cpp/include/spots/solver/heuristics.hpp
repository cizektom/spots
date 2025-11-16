#ifndef HEURISTICS_H
#define HEURISTICS_H

#include <cmath>
#include <memory>

#include "data_structures/couple.hpp"
#include "data_structures/proof_numbers.hpp"

namespace spots
{
    template <typename Game>
    struct Couple;

    namespace heuristics
    {
        template <typename Game>
        struct DefaultGameComparer
        {
            bool operator()(const Game &first, const Game &second) const;
        };

        template <typename Game>
        struct DefaultCoupleComparer
        {
            bool operator()(const spots::Couple<Game> &first, const Couple<Game> &second) const;
        };

        template <typename Game>
        struct ProofNumberEstimator
        {
            virtual ProofNumbers operator()(const Couple<Game> &couple) = 0;
        };
        template <typename Game>
        struct DefaultProofNumberEstimator : public ProofNumberEstimator<Game>
        {
            ProofNumbers operator()(const Couple<Game> &) override { return {1, 1}; }

            static std::shared_ptr<ProofNumberEstimator<Game>> create()
            {
                static std::shared_ptr<DefaultProofNumberEstimator<Game>> instance = std::make_shared<DefaultProofNumberEstimator<Game>>();
                return instance;
            }
        };

        template <typename Game>
        struct DepthProofNumberEstimator : public ProofNumberEstimator<Game>
        {
            ProofNumbers operator()(const Couple<Game> &couple) override;
            static std::shared_ptr<ProofNumberEstimator<Game>> create()
            {
                static std::shared_ptr<DepthProofNumberEstimator<Game>> instance = std::make_shared<DepthProofNumberEstimator<Game>>();
                return instance;
            }
        };
        template <typename Game>
        using DefaultEstimator = DefaultProofNumberEstimator<Game>;

        template <typename Game>
        using DepthEstimator = DepthProofNumberEstimator<Game>;

        template <typename Game>
        bool DefaultGameComparer<Game>::operator()(const Game &first, const Game &second) const
        {
            int firstLives = first.getLives();
            int secondLives = second.getLives();
            if (firstLives != secondLives)
                return firstLives < secondLives;

            size_t firstChildrenEstimation = first.estimateChildrenNumber();
            size_t secondChildrenEstimation = second.estimateChildrenNumber();
            if (firstChildrenEstimation != secondChildrenEstimation)
                return firstChildrenEstimation < secondChildrenEstimation;

            return first.to_string() < second.to_string(); // GLOP only compares positions
        }

        template <typename Game>
        bool DefaultCoupleComparer<Game>::operator()(const Couple<Game> &first, const Couple<Game> &second) const
        {
            const int nimberWeight = 4;
            int firstLives = first.position.getLives() + nimberWeight * first.nimber.value;
            int secondLives = second.position.getLives() + nimberWeight * second.nimber.value;
            if (firstLives != secondLives)
                return firstLives < secondLives;

            if (Game::isNormalImpartial)
            {
                size_t firstLandsNumber = first.position.getSubgamesNumber();
                size_t secondLandsNumber = second.position.getSubgamesNumber();
                if (firstLandsNumber != secondLandsNumber)
                    return firstLandsNumber > secondLandsNumber;
            }

            size_t firstChildrenEstimation = first.position.estimateChildrenNumber();
            size_t secondChildrenEstimation = second.position.estimateChildrenNumber();
            if (firstChildrenEstimation != secondChildrenEstimation)
                return firstChildrenEstimation < secondChildrenEstimation;

            return first.to_string() < second.to_string(); // GLOP only compares positions
        }

        template <typename Game>
        ProofNumbers DepthProofNumberEstimator<Game>::operator()(const Couple<Game> &c)
        {
            return {1 + c.estimateProofDepth(), 1 + c.estimateDisproofDepth()};
        }
    }
}

#endif
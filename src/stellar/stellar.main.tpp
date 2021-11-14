// ==========================================================================
//                    STELLAR - SwifT Exact LocaL AligneR
//                   http://www.seqan.de/projects/stellar/
// ==========================================================================
// Copyright (C) 2010-2012 by Birte Kehr
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your options) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ==========================================================================
// Author: Birte Kehr <birte.kehr@fu-berlin.de>
// ==========================================================================

#pragma once

#include "stellar.main.hpp"

#include <seqan/index.h>
#include <seqan/seq_io.h>

#include "../stellar.h"
#include "../stellar_output.h"

namespace stellar
{

using namespace seqan;

namespace app
{

///////////////////////////////////////////////////////////////////////////////
// Initializes a Finder object for a database sequence,
//  calls stellar, and writes matches to file
template <typename TAlphabet, typename TId, typename TStringSetSpec, typename TIndexSpec>
inline bool
_stellarOnOne(String<TAlphabet> const & database,
              TId const & databaseID,
              Pattern<Index<StringSet<String<TAlphabet>, TStringSetSpec> const, TIndexSpec>, Swift<SwiftLocal> > & swiftPattern,
              bool const databaseStrand,
              StringSet<QueryMatches<StellarMatch<String<TAlphabet> const, TId> > > & matches,
              StellarOptions & options)
{
    std::cout << "  " << databaseID;
    if (!databaseStrand)
        std::cout << ", complement";
    std::cout << std::flush;

    // finder
    using TSequence = String<TAlphabet>;
    typedef Finder<TSequence const, Swift<SwiftLocal> > TFinder;
    TFinder swiftFinder(database, options.minRepeatLength, options.maxRepeatPeriod);

    // stellar
    if (options.fastOption == CharString("exact"))
        stellar(swiftFinder, swiftPattern, options.epsilon, options.minLength, options.xDrop,
                options.disableThresh, options.compactThresh, options.numMatches, options.verbose,
                databaseID, databaseStrand, matches, AllLocal());
    else if (options.fastOption == "bestLocal")
        stellar(swiftFinder, swiftPattern, options.epsilon, options.minLength, options.xDrop,
                options.disableThresh, options.compactThresh, options.numMatches, options.verbose,
                databaseID, databaseStrand, matches, BestLocal());
    else if (options.fastOption == "bandedGlobal")
        stellar(swiftFinder, swiftPattern, options.epsilon, options.minLength, options.xDrop,
                options.disableThresh, options.compactThresh, options.numMatches, options.verbose,
                databaseID, databaseStrand, matches, BandedGlobal());
    else if (options.fastOption == "bandedGlobalExtend")
        stellar(swiftFinder, swiftPattern, options.epsilon, options.minLength, options.xDrop,
                options.disableThresh, options.compactThresh, options.numMatches, options.verbose,
                databaseID, databaseStrand, matches, BandedGlobalExtend());
    else
    {
        std::cerr << "\nUnknown verification strategy: " << options.fastOption << std::endl;
        return false;
    }

    std::cout << std::endl;
    return true;
}

} // namespace stellar::app

} // namespace stellar

//////////////////////////////////////////////////////////////////////////////
namespace seqan {

template <typename TStringSet, typename TShape, typename TSpec>
struct Cargo<Index<TStringSet, IndexQGram<TShape, TSpec> > >
{
    typedef struct
    {
        double      abundanceCut;
    } Type;
};

//////////////////////////////////////////////////////////////////////////////
// Repeat masker
template <typename TStringSet, typename TShape, typename TSpec>
inline bool _qgramDisableBuckets(Index<TStringSet, IndexQGram<TShape, TSpec> > & index)
{
    using TIndex = Index<TStringSet, IndexQGram<TShape, TSpec> >;
    using TDir = typename Fibre<TIndex, QGramDir>::Type;
    using TDirIterator = typename Iterator<TDir, Standard>::Type;
    using TSize = typename Value<TDir>::Type;

    TDir & dir    = indexDir(index);
    bool result  = false;
    unsigned counter = 0;
    TSize thresh = (TSize)(length(index) * cargo(index).abundanceCut);
    if (thresh < 100)
        thresh = 100;

    TDirIterator it = begin(dir, Standard());
    TDirIterator itEnd = end(dir, Standard());
    for (; it != itEnd; ++it)
        if (*it > thresh)
        {
            *it = (TSize) - 1;
            result = true;
            ++counter;
        }

    if (counter > 0)
        std::cerr << "Removed " << counter << " k-mers" << ::std::endl;

    return result;
}

template <>
struct FunctorComplement<AminoAcid>:
    public::std::function<AminoAcid(AminoAcid)>
{
    inline AminoAcid operator()(AminoAcid x) const
    {
        return x;
    }

};

} // namespace seqan

namespace stellar
{

namespace app
{

///////////////////////////////////////////////////////////////////////////////
// Initializes a Pattern object with the query sequences,
//  and calls _stellarOnOne for each database sequence
template <typename TSequence, typename TId>
inline bool
_stellarOnAll(StringSet<TSequence> & databases,
              StringSet<TId> const & databaseIDs,
              StringSet<TSequence> const & queries,
              StringSet<TId> const & queryIDs,
              StellarOptions & options)
{
    // pattern
    using TDependentQueries = StringSet<TSequence, Dependent<> > const;
    using TQGramIndex = Index<TDependentQueries, IndexQGram<SimpleShape, OpenAddressing> >;
    TDependentQueries dependentQueries{queries};
    TQGramIndex qgramIndex(dependentQueries);
    resize(indexShape(qgramIndex), options.qGram);
    cargo(qgramIndex).abundanceCut = options.qgramAbundanceCut;
    Pattern<TQGramIndex, Swift<SwiftLocal> > swiftPattern(qgramIndex);

    if (options.verbose)
        swiftPattern.params.printDots = true;

    // Construct index
    std::cout << "Constructing index..." << std::endl;
    indexRequire(qgramIndex, QGramSADir());
    std::cout << std::endl;

    // container for eps-matches
    StringSet<QueryMatches<StellarMatch<TSequence const, TId> > > matches;
    resize(matches, length(queries));

    std::cout << "Aligning all query sequences to database sequence..." << std::endl;
    for (unsigned i = 0; i < length(databases); ++i)
    {
        // positive database strand
        if (options.forward)
        {
            if (!_stellarOnOne(databases[i], databaseIDs[i], swiftPattern, true, matches, options))
                return 1;
        }
        // negative (reverse complemented) database strand
        if (options.reverse && options.alphabet != "protein" && options.alphabet != "char")
        {
            reverseComplement(databases[i]);
            if (!_stellarOnOne(databases[i], databaseIDs[i], swiftPattern, false, matches, options))
                return 1;

            reverseComplement(databases[i]);
        }
    }
    std::cout << std::endl;

    // file output
    if (options.disableThresh != std::numeric_limits<unsigned>::max())
    {
        if (!_outputMatches(matches, queries, queryIDs, databases, options.verbose,
                            options.outputFile, options.outputFormat, options.disabledQueriesFile))
            return 1;
    }
    else
    {
        if (!_outputMatches(matches, queryIDs, databases, options.verbose,
                            options.outputFile, options.outputFormat))
            return 1;
    }

    return 0;
}

template <typename TId>
inline bool
_checkUniqueId(std::set<TId> & uniqueIds, TId const & id)
{
    TId shortId;
    typedef typename Iterator<TId const>::Type TIterator;

    TIterator it = begin(id);
    TIterator itEnd = end(id);
    while (it != itEnd && *it > 32)
    {
        appendValue(shortId, *it);
        ++it;
    }

    if (uniqueIds.count(shortId) == 0)
    {
        uniqueIds.insert(shortId);
        return 1;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Imports sequences from a file,
//  stores them in the StringSet seqs and their identifiers in the StringSet ids
template <typename TSequence, typename TId>
inline bool
_importSequences(CharString const & fileName,
                 CharString const & name,
                 StringSet<TSequence> & seqs,
                 StringSet<TId> & ids)
{
    SeqFileIn inSeqs;
    if (!open(inSeqs, (toCString(fileName))))
    {
        std::cerr << "Failed to open " << name << " file." << std::endl;
        return false;
    }

    std::set<TId> uniqueIds; // set of short IDs (cut at first whitespace)
    bool idsUnique = true;

    TSequence seq;
    TId id;
    unsigned seqCount = 0;
    for (; !atEnd(inSeqs); ++seqCount)
    {
        readRecord(id, seq, inSeqs);

        idsUnique &= _checkUniqueId(uniqueIds, id);

        appendValue(seqs, seq, Generous());
        appendValue(ids, id, Generous());
    }

    std::cout << "Loaded " << seqCount << " " << name << " sequence" << ((seqCount > 1) ? "s." : ".") << std::endl;
    if (!idsUnique)
        std::cerr << "WARNING: Non-unique " << name << " ids. Output can be ambiguous.\n";
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Calculates parameters from parameters in options object and from sequences and writes them to std::cout
template <typename TStringSet>
void _writeMoreCalculatedParams(StellarOptions const & options, TStringSet const & databases, TStringSet const & queries)
{
//IOREV _notio_
    typedef typename Size<TStringSet const>::Type TSize;
    typedef typename Value<typename Value<TStringSet>::Type>::Type TAlphabet;

    if (options.qgramAbundanceCut != 1)
    {
        std::cout << "Calculated parameters:" << std::endl;
    }

    TSize queryLength = length(concat(queries));
    if (options.qgramAbundanceCut != 1)
    {
        std::cout << "  q-gram expected abundance : ";
        std::cout << queryLength / (double)((long)1 << (options.qGram << 1)) << std::endl;
        std::cout << "  q-gram abundance threshold: ";
        std::cout << _max(100, (int)(queryLength * options.qgramAbundanceCut)) << std::endl;
        std::cout << std::endl;
    }

    if (IsSameType<TAlphabet, Dna5>::VALUE || IsSameType<TAlphabet, Rna5>::VALUE)
    {
        // Computation of maximal E-value for this search

        TSize maxLengthQueries = 0;
        TSize maxLengthDatabases = 0;

        typename Iterator<TStringSet const>::Type dbIt = begin(databases);
        typename Iterator<TStringSet const>::Type dbEnd = end(databases);
        while (dbIt != dbEnd)
        {
            if (length(*dbIt) > maxLengthDatabases)
            {
                maxLengthDatabases = length(*dbIt);
            }
            ++dbIt;
        }

        typename Iterator<TStringSet const>::Type queriesIt = begin(queries);
        typename Iterator<TStringSet const>::Type queriesEnd = end(queries);
        while (queriesIt != queriesEnd)
        {
            if (length(*queriesIt) > maxLengthQueries)
            {
                maxLengthQueries = length(*queriesIt);
            }
            ++queriesIt;
        }

        TSize errors = static_cast<TSize>(options.minLength * options.epsilon);
        TSize minScore = options.minLength - 3 * errors; // #matches - 2*#errors // #matches = minLenght - errors,

        std::cout << "All matches matches resulting from your search have an E-value of: " << std::endl;
        std::cout << "        " << _computeEValue(minScore, maxLengthQueries, maxLengthDatabases) << " or smaller";
        std::cout << "  (match score = 1, error penalty = -2)" << std::endl;

        std::cout << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Calculates parameters from parameters in options object and writes them to std::cout
inline
void _writeCalculatedParams(StellarOptions & options)
{
//IOREV _notio_
    int errMinLen = (int) floor(options.epsilon * options.minLength);
    int n = (int) ceil((errMinLen + 1) / options.epsilon);
    int errN = (int) floor(options.epsilon * n);
    unsigned smin = (unsigned) _min(ceil((double)(options.minLength - errMinLen) / (errMinLen + 1)),
                                    ceil((double)(n - errN) / (errN + 1)));

    std::cout << "Calculated parameters:" << std::endl;
    if (options.qGram == (unsigned)-1)
    {
        options.qGram = (unsigned)_min(smin, 32u);
        std::cout << "  k-mer length : " << options.qGram << std::endl;
    }

    int threshold = (int) _max(1, (int) _min((n + 1) - options.qGram * (errN + 1),
                                             (options.minLength + 1) - options.qGram * (errMinLen + 1)));
    int overlap = (int) floor((2 * threshold + options.qGram - 3) / (1 / options.epsilon - options.qGram));
    int distanceCut = (threshold - 1) + options.qGram * overlap + options.qGram;
    int logDelta = _max(4, (int) ceil(log((double)overlap + 1) / log(2.0)));
    int delta = 1 << logDelta;

    std::cout << "  s^min        : " << smin << std::endl;
    std::cout << "  threshold    : " << threshold << std::endl;
    std::cout << "  distance cut : " << distanceCut << std::endl;
    std::cout << "  delta        : " << delta << std::endl;
    std::cout << "  overlap      : " << overlap << std::endl;
    std::cout << std::endl;
}

///////////////////////////////////////////////////////////////////////////////
// Writes user specified parameters from options object to std::cout
template <typename TOptions>
void
_writeSpecifiedParams(TOptions const & options)
{
//IOREV _notio_
    // Output user specified parameters
    std::cout << "User specified parameters:" << std::endl;
    std::cout << "  minimal match length             : " << options.minLength << std::endl;
    std::cout << "  maximal error rate (epsilon)     : " << options.epsilon << std::endl;
    std::cout << "  maximal x-drop                   : " << options.xDrop << std::endl;
    if (options.qGram != (unsigned)-1)
        std::cout << "  k-mer (q-gram) length            : " << options.qGram << std::endl;
    std::cout << "  search forward strand            : " << ((options.forward) ? "yes" : "no") << std::endl;
    std::cout << "  search reverse complement        : " << ((options.reverse) ? "yes" : "no") << std::endl;
    std::cout << std::endl;

    std::cout << "  verification strategy            : " << options.fastOption << std::endl;
    if (options.disableThresh != (unsigned)-1)
    {
        std::cout << "  disable queries with more than   : " << options.disableThresh << " matches" << std::endl;
    }
    std::cout << "  maximal number of matches        : " << options.numMatches << std::endl;
    std::cout << "  duplicate removal every          : " << options.compactThresh << std::endl;
    if (options.maxRepeatPeriod != 1 || options.minRepeatLength != 1000)
    {
        std::cout << "  max low complexity repeat period : " << options.maxRepeatPeriod << std::endl;
        std::cout << "  min low complexity repeat length : " << options.minRepeatLength << std::endl;
    }
    if (options.qgramAbundanceCut != 1)
    {
        std::cout << "  q-gram abundance cut ratio       : " << options.qgramAbundanceCut << std::endl;
    }
    std::cout << std::endl;
}

///////////////////////////////////////////////////////////////////////////////
// Writes file name from options object to std::cout
template <typename TOptions>
void
_writeFileNames(TOptions const & options)
{
//IOREV _notio_
    std::cout << "I/O options:" << std::endl;
    std::cout << "  database file   : " << options.databaseFile << std::endl;
    std::cout << "  query file      : " << options.queryFile << std::endl;
    std::cout << "  alphabet        : " << options.alphabet << std::endl;
    std::cout << "  output file     : " << options.outputFile << std::endl;
    std::cout << "  output format   : " << options.outputFormat << std::endl;
    if (options.disableThresh != (unsigned)-1)
    {
        std::cout << "  disabled queries: " << options.disabledQueriesFile << std::endl;
    }
    std::cout << std::endl;
}

///////////////////////////////////////////////////////////////////////////////
// Parses and outputs parameters, calls _stellarOnAll().
template <typename TAlphabet>
int mainWithOptions(StellarOptions & options, String<TAlphabet>)
{
    typedef String<TAlphabet> TSequence;

    // output file names
    stellar::app::_writeFileNames(options);

    // output parameters
    stellar::app::_writeSpecifiedParams(options);
    stellar::app::_writeCalculatedParams(options);

    // import query sequences
    StringSet<TSequence> queries;
    StringSet<CharString> queryIDs;
    if (!_importSequences(options.queryFile, "query", queries, queryIDs))
        return 1;

    // import database sequence
    StringSet<TSequence> databases;
    StringSet<CharString> databaseIDs;
    if (!_importSequences(options.databaseFile, "database", databases, databaseIDs))
        return 1;

    std::cout << std::endl;
    stellar::app::_writeMoreCalculatedParams(options, databases, queries);

    // open output files
    std::ofstream file;
    file.open(toCString(options.outputFile));
    if (!file.is_open())
    {
        std::cerr << "Could not open output file." << std::endl;
        return 1;
    }
    file.close();

    if (options.disableThresh != std::numeric_limits<unsigned>::max())
    {
        std::ofstream daFile;
        daFile.open(toCString(options.disabledQueriesFile));
        if (!daFile.is_open())
        {
            std::cerr << "Could not open file for disabled queries." << std::endl;
            return 1;
        }
        daFile.close();
    }

    // stellar on all databases and queries writing results to file

    double startTime = sysTime();
    if (!_stellarOnAll(databases, databaseIDs, queries, queryIDs, options))
        return 1;

    if (options.verbose && options.noRT == false)
        std::cout << "Running time: " << sysTime() - startTime << "s" << std::endl;

    return 0;
}

} // namespace stellar::app

} // namespace stellar

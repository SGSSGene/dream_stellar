#!/usr/bin/env python
"""Execute the tests for stellar.

The golden test outputs are generated by the script generate_outputs.sh.

You have to give the root paths to the source and the binaries as arguments to
the program.  These are the paths to the directory that contains the 'projects'
directory.

Usage:  run_tests.py SOURCE_ROOT_PATH BINARY_ROOT_PATH
"""
import logging
import os
import os.path
import sys

# Automagically add util/py_lib to PYTHONPATH environment variable.
path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..',
                                    '..', '..', 'util', 'py_lib'))
sys.path.insert(0, path)

import seqan.app_tests as app_tests

testsConfig = {
    'e-1': [
        '-e', '0.1', # --epsilon
        '-l', '50', # --minLength
        '-x', '10', # --xDrop
        '-k', '7', # --kmer
        '-n', '5000', # --numMatches
        '-s', '10000', # --sortThresh
        '-v', # --verbose
        '-no-rt', # --suppress-runtime-printing # for stable output
    ],
    '5e-2' : [
        '--epsilon', '0.05',
        '--minLength', '50',
        '--xDrop', '10',
        '--kmer', '7',
        '--numMatches', '5000',
        '--sortThresh', '10000',
        '--verbose',
        '--suppress-runtime-printing', # for stable output
    ],
    '25e-3' : [
        '--epsilon', '0.025',
        '--minLength', '50',
        '--xDrop', '10',
        '--kmer', '7',
        '--numMatches', '5000',
        '--sortThresh', '10000',
        '--verbose',
        '--suppress-runtime-printing', # for stable output
    ],
    '75e-3': [
        '--epsilon', '0.075',
        '--minLength', '50',
        '--xDrop', '10',
        '--kmer', '7',
        '--numMatches', '5000',
        '--sortThresh', '10000',
        '--verbose',
        '--suppress-runtime-printing', # for stable output
    ],
    'e-4' : [
        '--epsilon', '0.0001',
        '--minLength', '50',
        '--xDrop', '10',
        '--kmer', '7',
        '--numMatches', '5000',
        '--sortThresh', '10000',
        '--verbose',
        '--suppress-runtime-printing', # for stable output
    ],
    'minLen20' : [
        '--epsilon', '0.05',
        '--minLength', '20',
        '--xDrop', '10',
        '--kmer', '7',
        '--numMatches', '5000',
        '--sortThresh', '10000',
        '--verbose',
        '--suppress-runtime-printing', # for stable output
    ],
    'minLen150' : [
        '--epsilon', '0.05',
        '--minLength', '150',
        '--xDrop', '10',
        '--kmer', '7',
        '--numMatches', '5000',
        '--sortThresh', '10000',
        '--verbose',
        '--suppress-runtime-printing', # for stable output
    ]
}

class StellarTestSuite():

    def __init__(self, source_base, binary_base):
        self.shortFlags = {
            'forward': ['-f'],
            'reverse': ['-r'],
            'both': [],
            'dna': ['-a', 'dna'],
            'dna5': [], # in short flags we let dna5 be empty, since it the default value
            'protein': ['-a', 'protein'],
            'char': ['-a', 'char']
        }

        self.longFlags = {
            'forward': ['--forward'],
            'reverse': ['--reverse'],
            'both': [],
            'dna': ['--alphabet', 'dna'],
            'dna5': ['--alphabet', 'dna5'],
            'protein': ['--alphabet', 'protein'],
            'char': ['--alphabet', 'char']
        }

        # stellar/tests directory
        self.source_base = source_base
        self.binary_base = binary_base
        self.app_test_dir = os.path.join(source_base, 'test/cli') # original: 'apps/stellar/tests'
        self.relative_binary_path = "." # original: 'apps/stellar'

        self.pathHelper = app_tests.TestPathHelper(self.source_base, self.binary_base, self.app_test_dir)  # tests dir

        self.pathHelper.outFile('-')  # To ensure that the out path is set.

        # ============================================================
        # Built TestConf list.
        # ============================================================

        # Build list with TestConf objects, analoguely to how the output
        # was generated in generate_outputs.sh.
        self.tests = []

    def addTest(self, executable, errorRate, testName, alphabet, databaseStrand, outputExt, flags = None):
        flags = self.longFlags if flags is None else flags

        executable_file = app_tests.autolocateBinary(self.binary_base, self.relative_binary_path, executable)

        tmpSubDir = "{alphabet}_{databaseStrand}/".format(alphabet = alphabet, databaseStrand = databaseStrand)
        expectDataDir = self.pathHelper.inFile('gold_standard/%s' % tmpSubDir)

        testFormat = {
            'errorRate': errorRate,
            'testName': testName,
            'ext': outputExt,
            'expectDataDir': expectDataDir,
            'tmpSubDir': tmpSubDir
        }

        # We prepare a list of transforms to apply to the output files.  This is
        # used to strip the input/output paths from the programs' output to
        # make it more canonical and host independent.
        transforms = self.outputTransforms()

        test = app_tests.TestConf(
            program = executable_file,
            redir_stdout = self.pathHelper.outFile('{testName}.{ext}.stdout'.format(**testFormat), tmpSubDir),
            args =
                flags.get(alphabet, []) +
                flags.get(databaseStrand, []) +
                testsConfig[testName] +
                [
                    '--out', self.pathHelper.outFile('{testName}.{ext}'.format(**testFormat), tmpSubDir),
                    self.pathHelper.inFile('512_simSeq1_{errorRate}.fa'.format(**testFormat)),
                    self.pathHelper.inFile('512_simSeq2_{errorRate}.fa'.format(**testFormat))
                ],
            to_diff =
            [
                (
                    self.pathHelper.inFile('{expectDataDir}/{testName}.{ext}.stdout'.format(**testFormat)),
                    self.pathHelper.outFile('{testName}.{ext}.stdout'.format(**testFormat), tmpSubDir),
                    transforms
                ),
                (
                    self.pathHelper.inFile('{expectDataDir}/{testName}.{ext}'.format(**testFormat)),
                    self.pathHelper.outFile('{testName}.{ext}'.format(**testFormat), tmpSubDir),
                    transforms
                )
            ]
        )

        self.tests.append(test)

    def outputTransforms(self):
        return [
            app_tests.ReplaceTransform(os.path.join(self.pathHelper.source_base_path, self.app_test_dir) + os.sep, '', right=True),
            app_tests.ReplaceTransform(self.pathHelper.temp_dir + os.sep, '', right=True),
            app_tests.NormalizeScientificExponentsTransform(),
        ]

    def runTests(self):
        print('Executing test for stellar')
        print('=========================')
        print()

        # ============================================================
        # Execute the tests.
        # ============================================================
        failures = 0
        try:
            for test in self.tests:
                print(' '.join([test.program] + test.args))
                res = app_tests.runTest(test)
                # Output to the user.
                if res:
                     print('OK')
                else:
                    failures += 1
                    print('FAILED')
        except Exception as e:
            raise e # This exception is saved, then finally is executed, and then the exception is raised.
        finally:
            # Cleanup.
            self.pathHelper.deleteTempDir()

        print('==============================')
        print('     total tests: %d' % len(self.tests))
        print('    failed tests: %d' % failures)
        print('successful tests: %d' % (len(self.tests) - failures))
        print('==============================')

        return failures != 0


def main(source_base, binary_base, alphabets, database_strands, output_extensions):
    """Main entry point of the script."""

    testSuite = StellarTestSuite(source_base, binary_base)

    print ("alphabets:", alphabets)
    print ("database_strands:", database_strands)
    print ("output_extensions:", output_extensions)
    print ()

    # ============================================================
    # Run STELLAR.
    # ============================================================

    for alphabet in alphabets:
        for databaseStrand in database_strands:
            for outputExt in output_extensions:
                # Search complete database
                # Error rate 0.1:
                testSuite.addTest('stellar', errorRate = 'e-1', testName = 'e-1', alphabet = alphabet, databaseStrand = databaseStrand, outputExt = outputExt, flags = testSuite.shortFlags)

                # Error rate 0.05:
                testSuite.addTest('stellar', errorRate = '5e-2', testName = '5e-2', alphabet = alphabet, databaseStrand = databaseStrand, outputExt = outputExt)

                # Error rate 0.25:
                testSuite.addTest('stellar', errorRate = '25e-3', testName = '25e-3', alphabet = alphabet, databaseStrand = databaseStrand, outputExt = outputExt)

                # Error rate 0.75:
                testSuite.addTest('stellar', errorRate = '75e-3', testName = '75e-3', alphabet = alphabet, databaseStrand = databaseStrand, outputExt = outputExt)

                # Error rate 0.0001:
                testSuite.addTest('stellar', errorRate = 'e-4', testName = 'e-4', alphabet = alphabet, databaseStrand = databaseStrand, outputExt = outputExt)

                # Minimal length: 20, Error rate 0.05:
                testSuite.addTest('stellar', errorRate = '5e-2', testName = 'minLen20', alphabet = alphabet, databaseStrand = databaseStrand, outputExt = outputExt)

                # Minimal length: 150, Error rate 0.05:
                testSuite.addTest('stellar', errorRate = '5e-2', testName = 'minLen150', alphabet = alphabet, databaseStrand = databaseStrand, outputExt = outputExt)

    # Compute and return return code.
    return testSuite.runTests()

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(add_help=False)

    # --alphabets dna protein
    alphabets = ['dna', 'dna5', 'protein', 'char']
    parser.add_argument('--alphabets', nargs='*', default = alphabets, choices = alphabets)

    # --database-strands forward both
    database_strands = ['forward', 'reverse', 'both']
    parser.add_argument('--database-strands', nargs='*', default = database_strands, choices = database_strands)

    # --output-extensions txt gff
    output_extensions = ['gff', 'txt']
    parser.add_argument('--output-extensions', nargs='*', default = output_extensions, choices = output_extensions)

    (options, remaining_args) = parser.parse_known_args()

    # propagate remaining_args s.t. app_tests.main can use it
    sys.argv = [sys.argv[0]] + remaining_args

    sys.exit(app_tests.main(main,
                            alphabets = options.alphabets,
                            database_strands = options.database_strands,
                            output_extensions = options.output_extensions))

/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2011,2012,2013,2014,2015,2019, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <cstring>
using namespace std;

#include <gromacs/trajectoryanalysis.h>
using namespace gmx;

void executeCommand(string command, FILE* outfile=stdout) {
    FILE* fp;
    char path[PATH_MAX];
    int status;

    fp = popen(command.c_str(), "r");
    if (fp == NULL) {
        fprintf(outfile, "Error, pipe from popen returned null pointer");
    }
    while (fgets(path, PATH_MAX, fp) != NULL)
        fprintf(outfile, "%s", path);
    status = pclose(fp);
    if (status == -1) {
        printf("Error, child status from popen could not be obtained");
    }
}



/*! \brief
 * Template class to serve as a basis for user analysis tools.
 */
class AnalysisTemplate : public TrajectoryAnalysisModule
{
    public:
        AnalysisTemplate();

        virtual void initOptions(IOptionsContainer          *options,
                                 TrajectoryAnalysisSettings *settings);
        virtual void initAnalysis(const TrajectoryAnalysisSettings &settings,
                                  const TopologyInformation        &top);

        virtual void analyzeFrame(int frnr, const t_trxframe &fr, t_pbc *pbc,
                                  TrajectoryAnalysisModuleData *pdata);

        virtual void finishAnalysis(int nframes);
        virtual void writeOutput();

    private:
        class ModuleData;
        std::string                      custom_output_, groups_string_;
        // Selection                       refsel_;
        // SelectionList                   sel_;
};


AnalysisTemplate::AnalysisTemplate()
{
    // registerAnalysisDataset(&data_, "avedist");
}


void
AnalysisTemplate::initOptions(IOptionsContainer          *options,
                              TrajectoryAnalysisSettings *settings)
{
    static const char *const desc[] = {
        "Template which calls voro_interfaces++, an external program,",
        "to calculate voronoi interface area between groups",
        "have access to all information in the topology, and your",
        "program will be able to handle all types of coordinates and",
        "trajectory files supported by GROMACS. In addition,",
        "you get a lot of functionality for free from the trajectory",
        "analysis library, including support for flexible dynamic",
        "selections. Go ahead an try it![PAR]",
        "To get started with implementing your own analysis program,",
        "follow the instructions in the README file provided.",
        "This template implements a simple analysis programs that calculates",
        "average distances from a reference group to one or more",
        "analysis groups."
    };

    settings->setHelpText(desc);
    options->addOption(StringOption("c")
                              .store(&custom_output_)
                              .description("NOT IMPLEMENTED, Specify a custom output string"));
    options->addOption(StringOption("groups")
                              .store(&groups_string_)
                              .required()
                              .description(
        "Groups should be given as upper limit atomic id's specified in "
        "monotonically increasing order and hence must be contiguous in id number "
        "eg if you had  group1 from atom id's 1 to 100, group2 101 to 123, "
        "group3 124 to end you would give the string \"100 123\" (with explict quotation marks) "
        "Note: in GROMACS id's are 1-indexed, not 0-indexed "));
    // options->addOption(FileNameOption("o")
    //                        .filetype(eftPlot).outputFile()
    //                        .store(&fnDist_).defaultBasename("avedist")
    //                        .description("Average distances from reference group"));
    // options->addOption(SelectionOption("reference")
    //                        .store(&refsel_).required()
    //                        .description("Reference group to calculate distances from"));
    // options->addOption(SelectionOption("select")
    //                        .storeVector(&sel_).required().multiValue()
    //                        .description("Groups to calculate distances to"));

    // settings->setFlag(TrajectoryAnalysisSettings::efRequireTop);
}


void
AnalysisTemplate::initAnalysis(const TrajectoryAnalysisSettings &settings,
                               const TopologyInformation         &top)
{
    // Examples for accessing topology
    // top.atoms()[0].atom[0].m;
}


void
AnalysisTemplate::analyzeFrame(int frnr, const t_trxframe &fr, t_pbc *pbc,
                               TrajectoryAnalysisModuleData *pdata)
{
    string coords, command, pre_command;
    int dimension_scaling = 10; // x10 multipliers are needed as gromacs coords are in nm^2 while we output Angstrom^2
    /* voro++ requires a file format of
    atom_id x y z
    .gro files, from which the cutoff id's are chosen are indexed from 1
    and so we index here similarly
    */

    for (int i=0; i<fr.natoms; i++) {
        // i index loops atoms
        coords += to_string(i+1); 
        coords += " ";
        for (int j=0; j<3; j++) {
            // j index is for x, y and z
            coords += to_string(fr.x[i][j] * dimension_scaling);
            coords += " ";
        }
        coords += "\n";
    }

    float box_len = fr.box[0][0] * dimension_scaling; // Assume box is cubic
    pre_command = "voro_interfaces++ -stdin -stdout -sum -gp " + groups_string_ + " -p 0 " + to_string(box_len) + " 0 " + to_string(box_len) + " 0 " + to_string(box_len) + " file_name_placeholder <<< \"";
    command = pre_command + coords + "\"";   

    // Command check
    cout << "\n" << pre_command << "\n";
    // executeCommand(command);

}   


void
AnalysisTemplate::finishAnalysis(int /*nframes*/)
{
    // Things to do after having analysed all frames
}


void
AnalysisTemplate::writeOutput()
{
    // Can output things...
}

/*! \brief
 * The main function for the analysis template.
 */
int
main(int argc, char *argv[])
{
    return gmx::TrajectoryAnalysisCommandLineRunner::runAsMain<AnalysisTemplate>(argc, argv);
}

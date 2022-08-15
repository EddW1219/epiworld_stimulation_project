#ifndef EPIWORLD_SIR_H 
#define EPIWORLD_SIR_H

/**
 * @brief Template for a Susceptible-Infected-Removed (SIR) model
 * 
 * @param model A Model<TSeq> object where to set up the SIR.
 * @param vname std::string Name of the virus
 * @param initial_prevalence epiworld_double Initial prevalence
 * @param initial_efficacy epiworld_double Initial susceptibility_reduction of the immune system
 * @param initial_recovery epiworld_double Initial recovery rate of the immune system
 */
template<typename TSeq>
inline void sir(
    epiworld::Model<TSeq> & model,
    std::string vname,
    epiworld_double prevalence,
    epiworld_double infectiousness,
    epiworld_double recovery
    )
{

    // Adding statuses
    model.add_status("Susceptible", epiworld::default_update_susceptible<TSeq>);
    model.add_status("Infected", epiworld::default_update_exposed<TSeq>);
    model.add_status("Recovered");

    // Setting up parameters
    model.add_param(recovery, "Prob. of Recovery");
    model.add_param(infectiousness, "Infectiousness");

    // Preparing the virus -------------------------------------------
    epiworld::Virus<TSeq> virus(vname);
    virus.set_status(1,2,2);
    
    virus.set_prob_recovery(&model("Prob. of Recovery"));
    virus.set_prob_infecting(&model("Infectiousness"));
    
    model.add_virus(virus, prevalence);
   
}

template<typename TSeq = int>
inline epiworld::Model<TSeq> sir(
    std::string vname,
    epiworld_double prevalence,
    epiworld_double infectiousness,
    epiworld_double recovery
    )
{

    epiworld::Model<TSeq> model;

    sir(
        model,
        vname,
        prevalence,
        infectiousness,
        recovery
        );

    return model;

}

#endif

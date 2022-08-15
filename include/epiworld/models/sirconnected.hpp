#ifndef EPIWORLD_MODELS_SIRCONNECTED_HPP 
#define EPIWORLD_MODELS_SIRCONNECTED_HPP

class SIRCONSTATUS {
public:
    static const int SUSCEPTIBLE = 0;
    static const int INFECTED    = 1;
    static const int RECOVERED   = 2;
};

// Tracking who is infected and who is not
std::vector< epiworld::Agent<>* > tracked_agents_infected(0u);
std::vector< epiworld::Agent<>* > tracked_agents_infected_next(0u);

bool tracked_started = false;
int tracked_ninfected = 0;
int tracked_ninfected_next = 0;
epiworld_double tracked_current_infect_prob = 0.0;

template<typename TSeq>
inline void tracked_agents_check_init(epiworld::Model<TSeq> * m) 
{

    if (tracked_started)
        return;

    /* Checking first if it hasn't  */ 
    if (!tracked_started) 
    { 
        
        /* Listing who is infected */ 
        for (auto & p : *(m->get_agents()))
        {
            if (p.get_status() == SIRCONSTATUS::INFECTED)
            {
            
                tracked_agents_infected.push_back(&p);
                tracked_ninfected++;
            
            }
        }

        for (auto & p: tracked_agents_infected)
        {
            if (p->get_n_viruses() == 0)
                throw std::logic_error("Cannot be infected and have no viruses.");
        }
        
        tracked_started = true;

        // Computing infection probability
        tracked_current_infect_prob =  1.0 - std::pow(
            1.0 - (*m->p0) * (*m->p1) / m->size(),
            tracked_ninfected
        );
        
    }

}

EPI_NEW_UPDATEFUN(update_susceptible, int)
{

    tracked_agents_check_init(m);

    // No infected individual?
    if (tracked_ninfected == 0)
        return;

    if (m->runif() < tracked_current_infect_prob)
    {

        // Adding the individual to the queue
        tracked_agents_infected_next.push_back(p);
        tracked_ninfected_next++;

        // Now selecting who is transmitting the disease
        epiworld_fast_uint which = static_cast<epiworld_fast_uint>(
            std::floor(tracked_ninfected * m->runif())
        );

        // Infecting the individual
        p->add_virus(
            tracked_agents_infected[which]->get_virus(0u)
            ); 

        return;

    }

    return;

}

EPI_NEW_UPDATEFUN(update_infected, int)
{

    tracked_agents_check_init(m);

    // Is recovering
    if (m->runif() < (*m->p2))
    {

        tracked_ninfected_next--;
        epiworld::VirusPtr<int> v = p->get_virus(0u);
        p->rm_virus(0);
        return;

    }

    // Will be present next
    tracked_agents_infected_next.push_back(p);

    return;

}

EPI_NEW_GLOBALFUN(global_accounting, int)
{

    // On the last day, also reset tracked agents and
    // set the initialized value to false
    if (static_cast<unsigned int>(m->today()) == (m->get_ndays() - 1))
    {

        tracked_started = false;
        tracked_agents_infected.clear();
        tracked_agents_infected_next.clear();
        tracked_ninfected = 0;
        tracked_ninfected_next = 0;    
        tracked_current_infect_prob = 0.0;

        return;
    }

    std::swap(tracked_agents_infected, tracked_agents_infected_next);
    tracked_agents_infected_next.clear();

    tracked_ninfected += tracked_ninfected_next;
    tracked_ninfected_next = 0;

    tracked_current_infect_prob = 1.0 - std::pow(
        1.0 - (*m->p0) * (*m->p1) / m->size(),
        tracked_ninfected
        );

}



/**
 * @brief Template for a Susceptible-Infected-Removed (SIR) model
 * 
 * @param model A Model<TSeq> object where to set up the SIR.
 * @param vname std::string Name of the virus
 * @param prevalence Initial prevalence (proportion)
 * @param reproductive_number Reproductive number (beta)
 * @param prob_transmission Probability of transmission
 * @param prob_recovery Probability of recovery
 */
template<typename TSeq>
inline void set_up_sir_connected(
    epiworld::Model<TSeq> & model,
    std::string vname,
    epiworld_double prevalence,
    epiworld_double reproductive_number,
    epiworld_double prob_transmission,
    epiworld_double prob_recovery
    // epiworld_double prob_reinfection
    )
{

    // Status
    model.add_status("Susceptible", update_susceptible);
    model.add_status("Infected", update_infected);
    model.add_status("Recovered");

    // Setting up parameters
    model.add_param(reproductive_number, "Beta");
    model.add_param(prob_transmission, "Prob. Transmission");
    model.add_param(prob_recovery, "Prob. Recovery");
    // model.add_param(prob_reinfection, "Prob. Reinfection");
    
    // Preparing the virus -------------------------------------------
    epiworld::Virus<TSeq> virus(vname);
    virus.set_status(1, 2, 2);

    model.add_virus(virus, prevalence);

    // Adding updating function
    model.add_global_action(global_accounting, -1);

    model.queuing_off(); // No queuing need

    return;

}

#endif

#ifndef EPIWORLD_MODEL_MEAT_HPP
#define EPIWORLD_MODEL_MEAT_HPP



/**
 * @brief Function factory for saving model runs
 * 
 * @details This function is the default behavior of the `run_multiple`
 * member of `Model<TSeq>`. By default only the total history (
 * case counts by unit of time.)
 * 
 * @tparam TSeq 
 * @param fmt 
 * @param total_hist 
 * @param virus_info 
 * @param virus_hist 
 * @param tool_info 
 * @param tool_hist 
 * @param transmission 
 * @param transition 
 * @return std::function<void(size_t,Model<TSeq>*)> 
 */
template<typename TSeq = int>
inline std::function<void(size_t,Model<TSeq>*)> make_save_run(
    std::string fmt,
    bool total_hist,
    bool virus_info,
    bool virus_hist,
    bool tool_info,
    bool tool_hist,
    bool transmission,
    bool transition,
    bool reproductive,
    bool generation
    )
{

    // Counting number of %
    int n_fmt = 0;
    for (auto & f : fmt)
        if (f == '%')
            n_fmt++;

    if (n_fmt != 1)
        throw std::logic_error("The -fmt- argument must have only one \"%\" symbol.");

    // Listting things to save
    std::vector< bool > what_to_save = {
        virus_info,
        virus_hist,
        tool_info,
        tool_hist,
        total_hist,
        transmission,
        transition,
        reproductive,
        generation
    };

    std::function<void(size_t,Model<TSeq>*)> saver = [fmt,what_to_save](
        size_t niter, Model<TSeq> * m
    ) -> void {

        std::string virus_info = "";
        std::string virus_hist = "";
        std::string tool_info = "";
        std::string tool_hist = "";
        std::string total_hist = "";
        std::string transmission = "";
        std::string transition = "";
        std::string reproductive = "";
        std::string generation = "";

        char buff[128];
        if (what_to_save[0u])
        {
            virus_info = fmt + std::string("_virus_info.csv");
            snprintf(buff, sizeof(buff), virus_info.c_str(), niter);
            virus_info = buff;
        } 
        if (what_to_save[1u])
        {
            virus_hist = fmt + std::string("_virus_hist.csv");
            snprintf(buff, sizeof(buff), virus_hist.c_str(), niter);
            virus_hist = buff;
        } 
        if (what_to_save[2u])
        {
            tool_info = fmt + std::string("_tool_info.csv");
            snprintf(buff, sizeof(buff), tool_info.c_str(), niter);
            tool_info = buff;
        } 
        if (what_to_save[3u])
        {
            tool_hist = fmt + std::string("_tool_hist.csv");
            snprintf(buff, sizeof(buff), tool_hist.c_str(), niter);
            tool_hist = buff;
        } 
        if (what_to_save[4u])
        {
            total_hist = fmt + std::string("_total_hist.csv");
            snprintf(buff, sizeof(buff), total_hist.c_str(), niter);
            total_hist = buff;
        } 
        if (what_to_save[5u])
        {
            transmission = fmt + std::string("_transmission.csv");
            snprintf(buff, sizeof(buff), transmission.c_str(), niter);
            transmission = buff;
        } 
        if (what_to_save[6u])
        {
            transition = fmt + std::string("_transition.csv");
            snprintf(buff, sizeof(buff), transition.c_str(), niter);
            transition = buff;
        } 
        if (what_to_save[7u])
        {

            reproductive = fmt + std::string("_reproductive.csv");
            snprintf(buff, sizeof(buff), reproductive.c_str(), niter);
            reproductive = buff;

        }
        if (what_to_save[8u])
        {

            generation = fmt + std::string("_generation.csv");
            snprintf(buff, sizeof(buff), generation.c_str(), niter);
            generation = buff;

        }
        
    
        m->write_data(
            virus_info,
            virus_hist,
            tool_info,
            tool_hist,
            total_hist,
            transmission,
            transition,
            reproductive,
            generation
        );

    };

    return saver;
}

template<typename TSeq>
inline void Model<TSeq>::actions_add(
    Agent<TSeq> * agent_,
    VirusPtr<TSeq> virus_,
    ToolPtr<TSeq> tool_,
    Entity<TSeq> * entity_,
    epiworld_fast_uint new_state_,
    epiworld_fast_int queue_,
    ActionFun<TSeq> call_,
    int idx_agent_,
    int idx_object_
) {
    
    ++nactions;

    #ifdef EPI_DEBUG
    if (nactions == 0)
        throw std::logic_error("Actions cannot be zero!!");

    if ((virus_ != nullptr) && idx_agent_ >= 0)
    {
        if (idx_agent_ >= static_cast<int>(virus_->get_agent()->get_n_viruses()))
            throw std::logic_error(
                "The virus to add is out of range in the host agent."
                );
    }
    #endif

    if (nactions > actions.size())
    {

        actions.push_back(
            Action<TSeq>(
                agent_, virus_, tool_, entity_, new_state_, queue_, call_,
                idx_agent_, idx_object_
            ));

    }
    else 
    {

        Action<TSeq> & A = actions.at(nactions - 1u);

        A.agent      = agent_;
        A.virus      = virus_;
        A.tool       = tool_;
        A.entity     = entity_;
        A.new_state = new_state_;
        A.queue      = queue_;
        A.call       = call_;
        A.idx_agent  = idx_agent_;
        A.idx_object = idx_object_;

    }

    return;

}

template<typename TSeq>
inline void Model<TSeq>::actions_run()
{
    // Making the call
    while (nactions != 0u)
    {

        Action<TSeq>   a = actions[--nactions];
        Agent<TSeq> * p  = a.agent;

        // Applying function
        if (a.call)
        {
            a.call(a, this);
        }

        // Updating state
        if (static_cast<epiworld_fast_int>(p->state) != a.new_state)
        {

            if (a.new_state >= static_cast<epiworld_fast_int>(nstates))
                throw std::range_error(
                    "The proposed state " + std::to_string(a.new_state) + " is out of range. " +
                    "The model currently has " + std::to_string(nstates - 1) + " states.");

            // Figuring out if we need to undo a change
            // If the agent has made a change in the state recently, then we
            // need to undo the accounting, e.g., if A->B was made, we need to
            // undo it and set B->A so that the daily accounting is right.
            if (p->state_last_changed == today())
            {

                // Updating accounting
                db.update_state(p->state_prev, p->state, true); // Undoing
                db.update_state(p->state_prev, a.new_state);

                for (size_t v = 0u; v < p->n_viruses; ++v)
                {
                    db.update_virus(p->viruses[v]->id, p->state, p->state_prev); // Undoing
                    db.update_virus(p->viruses[v]->id, p->state_prev, a.new_state);
                }

                for (size_t t = 0u; t < p->n_tools; ++t)
                {
                    db.update_tool(p->tools[t]->id, p->state, p->state_prev); // Undoing
                    db.update_tool(p->tools[t]->id, p->state_prev, a.new_state);
                }

                // Changing to the new state, we won't update the
                // previous state in case we need to undo the change
                p->state = a.new_state;

            } else {

                // Updating accounting
                db.update_state(p->state, a.new_state);

                for (size_t v = 0u; v < p->n_viruses; ++v)
                    db.update_virus(p->viruses[v]->id, p->state, a.new_state);

                for (size_t t = 0u; t < p->n_tools; ++t)
                    db.update_tool(p->tools[t]->id, p->state, a.new_state);


                // Saving the last state and setting the new one
                p->state_prev = p->state;
                p->state      = a.new_state;

                // It used to be a day before, but we still
                p->state_last_changed = today();

            }
            
        }

        #ifdef EPI_DEBUG
        if (static_cast<int>(p->state) >= static_cast<int>(nstates))
                throw std::range_error(
                    "The new state " + std::to_string(p->state) + " is out of range. " +
                    "The model currently has " + std::to_string(nstates - 1) + " states.");
        #endif

        // Updating queue
        if (use_queuing)
        {

            if (a.queue == Queue<TSeq>::Everyone)
                queue += p;
            else if (a.queue == -Queue<TSeq>::Everyone)
                queue -= p;
            else if (a.queue == Queue<TSeq>::OnlySelf)
                queue[p->get_id()]++;
            else if (a.queue == -Queue<TSeq>::OnlySelf)
                queue[p->get_id()]--;
            else if (a.queue != Queue<TSeq>::NoOne)
                throw std::logic_error(
                    "The proposed queue change is not valid. Queue values can be {-2, -1, 0, 1, 2}."
                    );
                    
        }

    }

    return;
    
}

/**
 * @name Default function for combining susceptibility_reduction levels
 * 
 * @tparam TSeq 
 * @param pt 
 * @return epiworld_double 
 */
///@{
template<typename TSeq>
inline epiworld_double susceptibility_reduction_mixer_default(
    Agent<TSeq>* p,
    VirusPtr<TSeq> v,
    Model<TSeq> * m
)
{
    epiworld_double total = 1.0;
    for (auto & tool : p->get_tools())
        total *= (1.0 - tool->get_susceptibility_reduction(v, m));

    return 1.0 - total;
    
}

template<typename TSeq>
inline epiworld_double transmission_reduction_mixer_default(
    Agent<TSeq>* p,
    VirusPtr<TSeq> v,
    Model<TSeq>* m
)
{
    epiworld_double total = 1.0;
    for (auto & tool : p->get_tools())
        total *= (1.0 - tool->get_transmission_reduction(v, m));

    return (1.0 - total);
    
}

template<typename TSeq>
inline epiworld_double recovery_enhancer_mixer_default(
    Agent<TSeq>* p,
    VirusPtr<TSeq> v,
    Model<TSeq>* m
)
{
    epiworld_double total = 1.0;
    for (auto & tool : p->get_tools())
        total *= (1.0 - tool->get_recovery_enhancer(v, m));

    return 1.0 - total;
    
}

template<typename TSeq>
inline epiworld_double death_reduction_mixer_default(
    Agent<TSeq>* p,
    VirusPtr<TSeq> v,
    Model<TSeq>* m
) {

    epiworld_double total = 1.0;
    for (auto & tool : p->get_tools())
    {
        total *= (1.0 - tool->get_death_reduction(v, m));
    } 

    return 1.0 - total;
    
}
///@}

template<typename TSeq>
inline Model<TSeq> * Model<TSeq>::clone_ptr()
{
    Model<TSeq> * ptr = new Model<TSeq>(*dynamic_cast<const Model<TSeq>*>(this));

    #ifdef EPI_DEBUG
    if (*this != *ptr)
        throw std::logic_error("Model::clone_ptr The copies of the model don't match.");
    #endif

    return ptr;
}

template<typename TSeq>
inline Model<TSeq>::Model()
{
    db.model = this;
    db.user_data = this;
    if (use_queuing)
        queue.model = this;
}

template<typename TSeq>
inline Model<TSeq>::Model(const Model<TSeq> & model) :
    name(model.name),
    db(model.db),
    population(model.population),
    population_backup(model.population_backup),
    directed(model.directed),
    viruses(model.viruses),
    prevalence_virus(model.prevalence_virus),
    prevalence_virus_as_proportion(model.prevalence_virus_as_proportion),
    viruses_dist_funs(model.viruses_dist_funs),
    tools(model.tools),
    prevalence_tool(model.prevalence_tool),
    prevalence_tool_as_proportion(model.prevalence_tool_as_proportion),
    tools_dist_funs(model.tools_dist_funs),
    entities(model.entities),
    entities_backup(model.entities_backup),
    // prevalence_entity(model.prevalence_entity),
    // prevalence_entity_as_proportion(model.prevalence_entity_as_proportion),
    // entities_dist_funs(model.entities_dist_funs),
    rewire_fun(model.rewire_fun),
    rewire_prop(model.rewire_prop),
    parameters(model.parameters),
    ndays(model.ndays),
    pb(model.pb),
    state_fun(model.state_fun),
    states_labels(model.states_labels),
    nstates(model.nstates),
    verbose(model.verbose),
    current_date(model.current_date),
    global_actions(model.global_actions),
    queue(model.queue),
    use_queuing(model.use_queuing),
    array_double_tmp(model.array_double_tmp.size()),
    array_virus_tmp(model.array_virus_tmp.size())
{


    // Removing old neighbors
    for (auto & p : population)
        p.model = this;

    if (population_backup.size() != 0u)
        for (auto & p : population_backup)
            p.model = this;

    for (auto & e : entities)
        e.model = this;

    if (entities_backup.size() != 0u)
        for (auto & e : entities_backup)
            e.model = this;

    // Pointing to the right place. This needs
    // to be done afterwards since the state zero is set as a function
    // of the population.
    db.model = this;
    db.user_data.model = this;

    if (use_queuing)
        queue.model = this;

    agents_data = model.agents_data;
    agents_data_ncols = model.agents_data_ncols;

}

template<typename TSeq>
inline Model<TSeq>::Model(Model<TSeq> && model) :
    name(std::move(model.name)),
    db(std::move(model.db)),
    population(std::move(model.population)),
    agents_data(std::move(model.agents_data)),
    agents_data_ncols(std::move(model.agents_data_ncols)),
    directed(std::move(model.directed)),
    // Virus
    viruses(std::move(model.viruses)),
    prevalence_virus(std::move(model.prevalence_virus)),
    prevalence_virus_as_proportion(std::move(model.prevalence_virus_as_proportion)),
    viruses_dist_funs(std::move(model.viruses_dist_funs)),
    // Tools
    tools(std::move(model.tools)),
    prevalence_tool(std::move(model.prevalence_tool)),
    prevalence_tool_as_proportion(std::move(model.prevalence_tool_as_proportion)),
    tools_dist_funs(std::move(model.tools_dist_funs)),
    // Entities
    entities(std::move(model.entities)),
    entities_backup(std::move(model.entities_backup)),
    // prevalence_entity(std::move(model.prevalence_entity)),
    // prevalence_entity_as_proportion(std::move(model.prevalence_entity_as_proportion)),
    // entities_dist_funs(std::move(model.entities_dist_funs)),
    // Pseudo-RNG
    engine(std::move(model.engine)),
    runifd(std::move(model.runifd)),
    rnormd(std::move(model.rnormd)),
    rgammad(std::move(model.rgammad)),
    rlognormald(std::move(model.rlognormald)),
    rexpd(std::move(model.rexpd)),
    // Rewiring
    rewire_fun(std::move(model.rewire_fun)),
    rewire_prop(std::move(model.rewire_prop)),
    parameters(std::move(model.parameters)),
    // Others
    ndays(model.ndays),
    pb(std::move(model.pb)),
    state_fun(std::move(model.state_fun)),
    states_labels(std::move(model.states_labels)),
    nstates(model.nstates),
    verbose(model.verbose),
    current_date(std::move(model.current_date)),
    global_actions(std::move(model.global_actions)),
    queue(std::move(model.queue)),
    use_queuing(model.use_queuing),
    array_double_tmp(model.array_double_tmp.size()),
    array_virus_tmp(model.array_virus_tmp.size())
{

    db.model = this;
    db.user_data.model = this;

    if (use_queuing)
        queue.model = this;

}

template<typename TSeq>
inline Model<TSeq> & Model<TSeq>::operator=(const Model<TSeq> & m)
{
    name = m.name;

    population        = m.population;
    population_backup = m.population_backup;

    for (auto & p : population)
        p.model = this;

    if (population_backup.size() != 0)
        for (auto & p : population_backup)
            p.model = this;

    for (auto & e : entities)
        e.model = this;

    if (entities_backup.size() != 0)
        for (auto & e : entities_backup)
            e.model = this;

    db = m.db;
    db.model = this;
    db.user_data.model = this;

    directed = m.directed;
    
    viruses                        = m.viruses;
    prevalence_virus               = m.prevalence_virus;
    prevalence_virus_as_proportion = m.prevalence_virus_as_proportion;
    viruses_dist_funs              = m.viruses_dist_funs;

    tools                         = m.tools;
    prevalence_tool               = m.prevalence_tool;
    prevalence_tool_as_proportion = m.prevalence_tool_as_proportion;
    tools_dist_funs               = m.tools_dist_funs;
    
    entities        = m.entities;
    entities_backup = m.entities_backup;
    // prevalence_entity = m.prevalence_entity;
    // prevalence_entity_as_proportion = m.prevalence_entity_as_proportion;
    // entities_dist_funs = m.entities_dist_funs;
    
    rewire_fun  = m.rewire_fun;
    rewire_prop = m.rewire_prop;

    parameters = m.parameters;
    ndays      = m.ndays;
    pb         = m.pb;

    state_fun    = m.state_fun;
    states_labels = m.states_labels;
    nstates       = m.nstates;

    verbose     = m.verbose;

    current_date = m.current_date;

    global_actions = m.global_actions;

    queue       = m.queue;
    use_queuing = m.use_queuing;

    // Making sure population is passed correctly
    // Pointing to the right place
    db.model = this;
    db.user_data.model = this;

    agents_data            = m.agents_data;
    agents_data_ncols = m.agents_data_ncols;

    // Figure out the queuing
    if (use_queuing)
        queue.model = this;

    array_double_tmp.resize(std::max(
        size(),
        static_cast<size_t>(1024 * 1024)
    ));

    array_virus_tmp.resize(1024u);

    return *this;

}

template<typename TSeq>
inline DataBase<TSeq> & Model<TSeq>::get_db()
{
    return db;
}

template<typename TSeq>
inline std::vector<Agent<TSeq>> & Model<TSeq>::get_agents()
{
    return population;
}

template<typename TSeq>
inline std::vector< epiworld_fast_uint > Model<TSeq>::get_agents_states() const
{
    std::vector< epiworld_fast_uint > states(population.size());
    for (size_t i = 0u; i < population.size(); ++i)
        states[i] = population[i].get_state();

    return states;
}

template<typename TSeq>
inline std::vector< Viruses_const<TSeq> > Model<TSeq>::get_agents_viruses() const
{

    std::vector< Viruses_const<TSeq> > viruses(population.size());
    for (size_t i = 0u; i < population.size(); ++i)
        viruses[i] = population[i].get_viruses();

    return viruses;

}

// Same as before, but the non const version
template<typename TSeq>
inline std::vector< Viruses<TSeq> > Model<TSeq>::get_agents_viruses()
{

    std::vector< Viruses<TSeq> > viruses(population.size());
    for (size_t i = 0u; i < population.size(); ++i)
        viruses[i] = population[i].get_viruses();

    return viruses;

}

template<typename TSeq>
inline std::vector<Entity<TSeq>> & Model<TSeq>::get_entities()
{
    return entities;
}

template<typename TSeq>
inline void Model<TSeq>::agents_smallworld(
    epiworld_fast_uint n,
    epiworld_fast_uint k,
    bool d,
    epiworld_double p
)
{
    agents_from_adjlist(
        rgraph_smallworld(n, k, p, d, *this)
    );
}

template<typename TSeq>
inline void Model<TSeq>::agents_empty_graph(
    epiworld_fast_uint n
) 
{

    // Resizing the people
    population.clear();
    population.resize(n, Agent<TSeq>());

    // Filling the model and ids
    size_t i = 0u;
    for (auto & p : population)
    {
        p.id = i++;
        p.model = this;
    }
    

}

// template<typename TSeq>
// inline void Model<TSeq>::set_rand_engine(std::mt19937 & eng)
// {
//     engine = std::make_shared< std::mt19937 >(eng);
// }

template<typename TSeq>
inline void Model<TSeq>::set_rand_gamma(epiworld_double alpha, epiworld_double beta)
{
    rgammad = std::gamma_distribution<>(alpha,beta);
}

template<typename TSeq>
inline void Model<TSeq>::set_rand_norm(epiworld_double mean, epiworld_double sd)
{ 
    rnormd  = std::normal_distribution<>(mean, sd);
}

template<typename TSeq>
inline void Model<TSeq>::set_rand_unif(epiworld_double a, epiworld_double b)
{ 
    runifd  = std::uniform_real_distribution<>(a, b);
}

template<typename TSeq>
inline void Model<TSeq>::set_rand_lognormal(epiworld_double mean, epiworld_double shape)
{ 
    rlognormald  = std::lognormal_distribution<>(mean, shape);
}

template<typename TSeq>
inline void Model<TSeq>::set_rand_exp(epiworld_double lambda)
{ 
    rexpd  = std::exponential_distribution<>(lambda);
}

template<typename TSeq>
inline void Model<TSeq>::set_rand_binom(int n, epiworld_double p)
{ 
    rbinomd  = std::binomial_distribution<>(n, p);
}

template<typename TSeq>
inline epiworld_double & Model<TSeq>::operator()(std::string pname) {

    if (parameters.find(pname) == parameters.end())
        throw std::range_error("The parameter "+ pname + "is not in the model.");

    return parameters[pname];

}

template<typename TSeq>
inline size_t Model<TSeq>::size() const {
    return population.size();
}

template<typename TSeq>
inline void Model<TSeq>::dist_virus()
{

    // Starting first infection
    int n = size();
    std::vector< size_t > idx(n);

    int n_left = n;
    std::iota(idx.begin(), idx.end(), 0);

    for (size_t v = 0u; v < viruses.size(); ++v)
    {

        if (viruses_dist_funs[v])
        {

            viruses_dist_funs[v](*viruses[v], this);

        } else {

            // Picking how many
            int nsampled;
            if (prevalence_virus_as_proportion[v])
            {
                nsampled = static_cast<int>(std::floor(prevalence_virus[v] * size()));
            }
            else
            {
                nsampled = static_cast<int>(prevalence_virus[v]);
            }

            if (nsampled > static_cast<int>(size()))
                throw std::range_error("There are only " + std::to_string(size()) + 
                " individuals in the population. Cannot add the virus to " + std::to_string(nsampled));


            VirusPtr<TSeq> virus = viruses[v];
            
            while (nsampled > 0)
            {

                int loc = static_cast<epiworld_fast_uint>(floor(runif() * (n_left--)));

                Agent<TSeq> & agent = population[idx[loc]];
                
                // Adding action
                agent.add_virus(
                    virus,
                    const_cast<Model<TSeq> * >(this),
                    virus->state_init,
                    virus->queue_init
                    );

                // Adjusting sample
                nsampled--;
                std::swap(idx[loc], idx[n_left]);

            }

        }

        // Apply the actions
        actions_run();
    }

}

template<typename TSeq>
inline void Model<TSeq>::dist_tools()
{

    // Starting first infection
    int n = size();
    std::vector< size_t > idx(n);
    for (epiworld_fast_uint t = 0; t < tools.size(); ++t)
    {

        if (tools_dist_funs[t])
        {

            tools_dist_funs[t](*tools[t], this);

        } else {

            // Picking how many
            int nsampled;
            if (prevalence_tool_as_proportion[t])
            {
                nsampled = static_cast<int>(std::floor(prevalence_tool[t] * size()));
            }
            else
            {
                nsampled = static_cast<int>(prevalence_tool[t]);
            }

            if (nsampled > static_cast<int>(size()))
                throw std::range_error("There are only " + std::to_string(size()) + 
                " individuals in the population. Cannot add the tool to " + std::to_string(nsampled));
            
            ToolPtr<TSeq> tool = tools[t];

            int n_left = n;
            std::iota(idx.begin(), idx.end(), 0);
            while (nsampled > 0)
            {
                int loc = static_cast<epiworld_fast_uint>(floor(runif() * n_left--));
                
                population[idx[loc]].add_tool(
                    tool,
                    const_cast< Model<TSeq> * >(this),
                    tool->state_init, tool->queue_init
                    );
                
                nsampled--;

                std::swap(idx[loc], idx[n_left]);

            }

        }

        // Apply the actions
        actions_run();

    }

}

// template<typename TSeq>
// inline void Model<TSeq>::dist_entities()
// {

//     // Starting first infection
//     int n = size();
//     std::vector< size_t > idx(n);
//     for (epiworld_fast_uint e = 0; e < entities.size(); ++e)
//     {

//         if (entities_dist_funs[e])
//         {

//             entities_dist_funs[e](entities[e], this);

//         } else {

//             // Picking how many
//             int nsampled;
//             if (prevalence_entity_as_proportion[e])
//             {
//                 nsampled = static_cast<int>(std::floor(prevalence_entity[e] * size()));
//             }
//             else
//             {
//                 nsampled = static_cast<int>(prevalence_entity[e]);
//             }

//             if (nsampled > static_cast<int>(size()))
//                 throw std::range_error("There are only " + std::to_string(size()) + 
//                 " individuals in the population. Cannot add the entity to " + std::to_string(nsampled));
            
//             Entity<TSeq> & entity = entities[e];

//             int n_left = n;
//             std::iota(idx.begin(), idx.end(), 0);
//             while (nsampled > 0)
//             {
//                 int loc = static_cast<epiworld_fast_uint>(floor(runif() * n_left--));
                
//                 population[idx[loc]].add_entity(entity, this, entity.state_init, entity.queue_init);
                
//                 nsampled--;

//                 std::swap(idx[loc], idx[n_left]);

//             }

//         }

//         // Apply the actions
//         actions_run();

//     }

// }

template<typename TSeq>
inline void Model<TSeq>::chrono_start() {
    time_start = std::chrono::steady_clock::now();
}

template<typename TSeq>
inline void Model<TSeq>::chrono_end() {
    time_end = std::chrono::steady_clock::now();
    time_elapsed += (time_end - time_start);
    n_replicates++;
}

template<typename TSeq>
inline void Model<TSeq>::set_backup()
{

    if (population_backup.size() == 0u)
        population_backup = population;

    if (entities_backup.size() == 0u)
        entities_backup = entities;

}

// template<typename TSeq>
// inline void Model<TSeq>::restore_backup()
// {

//     // Restoring the data
//     population = *population_backup;
//     entities   = *entities_backup;

//     // And correcting the pointer
//     for (auto & p : population)
//         p.model = this;

//     for (auto & e : entities)
//         e.model = this;

// }

template<typename TSeq>
inline std::mt19937 & Model<TSeq>::get_rand_endgine()
{
    return engine;
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::runif() {
    // CHECK_INIT()
    return runifd(engine);
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::runif(epiworld_double a, epiworld_double b) {
    // CHECK_INIT()
    return runifd(engine) * (b - a) + a;
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::rnorm() {
    // CHECK_INIT()
    return rnormd(engine);
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::rnorm(epiworld_double mean, epiworld_double sd) {
    // CHECK_INIT()
    return rnormd(engine) * sd + mean;
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::rgamma() {
    return rgammad(engine);
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::rgamma(epiworld_double alpha, epiworld_double beta) {
    auto old_param = rgammad.param();
    rgammad.param(std::gamma_distribution<>::param_type(alpha, beta));
    epiworld_double ans = rgammad(engine);
    rgammad.param(old_param);
    return ans;
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::rexp() {
    return rexpd(engine);
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::rexp(epiworld_double lambda) {
    auto old_param = rexpd.param();
    rexpd.param(std::exponential_distribution<>::param_type(lambda));
    epiworld_double ans = rexpd(engine);
    rexpd.param(old_param);
    return ans;
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::rlognormal() {
    return rlognormald(engine);
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::rlognormal(epiworld_double mean, epiworld_double shape) {
    auto old_param = rlognormald.param();
    rlognormald.param(std::lognormal_distribution<>::param_type(mean, shape));
    epiworld_double ans = rlognormald(engine);
    rlognormald.param(old_param);
    return ans;
}

template<typename TSeq>
inline int Model<TSeq>::rbinom() {
    return rbinomd(engine);
}

template<typename TSeq>
inline int Model<TSeq>::rbinom(int n, epiworld_double p) {
    auto old_param = rbinomd.param();
    rbinomd.param(std::binomial_distribution<>::param_type(n, p));
    epiworld_double ans = rbinomd(engine);
    rbinomd.param(old_param);
    return ans;
}

template<typename TSeq>
inline void Model<TSeq>::seed(size_t s) {
    this->engine.seed(s);
}

template<typename TSeq>
inline void Model<TSeq>::add_virus(Virus<TSeq> & v, epiworld_double preval)
{

    if (preval > 1.0)
        throw std::range_error("Prevalence of virus cannot be above 1.0");

    if (preval < 0.0)
        throw std::range_error("Prevalence of virus cannot be negative");

    // Checking the state
    epiworld_fast_int init_, post_, rm_;
    v.get_state(&init_, &post_, &rm_);

    if (init_ == -99)
        throw std::logic_error(
            "The virus \"" + v.get_name() + "\" has no -init- state."
            );
    else if (post_ == -99)
        throw std::logic_error(
            "The virus \"" + v.get_name() + "\" has no -post- state."
            );
    
    // Recording the variant
    db.record_virus(v);

    // Adding new virus
    viruses.push_back(std::make_shared< Virus<TSeq> >(v));
    prevalence_virus.push_back(preval);
    prevalence_virus_as_proportion.push_back(true);
    viruses_dist_funs.push_back(nullptr);

}

template<typename TSeq>
inline void Model<TSeq>::add_virus_n(Virus<TSeq> & v, epiworld_fast_uint preval)
{

    // Checking the ids
    epiworld_fast_int init_, post_, rm_;
    v.get_state(&init_, &post_, &rm_);

    if (init_ == -99)
        throw std::logic_error(
            "The virus \"" + v.get_name() + "\" has no -init- state."
            );
    else if (post_ == -99)
        throw std::logic_error(
            "The virus \"" + v.get_name() + "\" has no -post- state."
            );

    // Setting the id
    db.record_virus(v);

    // Adding new virus
    viruses.push_back(std::make_shared< Virus<TSeq> >(v));
    prevalence_virus.push_back(preval);
    prevalence_virus_as_proportion.push_back(false);
    viruses_dist_funs.push_back(nullptr);

}

template<typename TSeq>
inline void Model<TSeq>::add_virus_fun(Virus<TSeq> & v, VirusToAgentFun<TSeq> fun)
{

    // Checking the ids
    epiworld_fast_int init_, post_, rm_;
    v.get_state(&init_, &post_, &rm_);

    if (init_ == -99)
        throw std::logic_error(
            "The virus \"" + v.get_name() + "\" has no -init- state."
            );
    else if (post_ == -99)
        throw std::logic_error(
            "The virus \"" + v.get_name() + "\" has no -post- state."
            );

    // Setting the id
    db.record_virus(v);
    // v.set_id(viruses.size());

    // Adding new virus
    viruses.push_back(std::make_shared< Virus<TSeq> >(v));
    prevalence_virus.push_back(0.0);
    prevalence_virus_as_proportion.push_back(false);
    viruses_dist_funs.push_back(fun);

}

template<typename TSeq>
inline void Model<TSeq>::add_tool(Tool<TSeq> & t, epiworld_double preval)
{

    if (preval > 1.0)
        throw std::range_error("Prevalence of tool cannot be above 1.0");

    if (preval < 0.0)
        throw std::range_error("Prevalence of tool cannot be negative");

    db.record_tool(t);

    // Adding the tool to the model (and database.)
    tools.push_back(std::make_shared< Tool<TSeq> >(t));
    prevalence_tool.push_back(preval);
    prevalence_tool_as_proportion.push_back(true);
    tools_dist_funs.push_back(nullptr);

}

template<typename TSeq>
inline void Model<TSeq>::add_tool_n(Tool<TSeq> & t, epiworld_fast_uint preval)
{
    
    db.record_tool(t);

    tools.push_back(std::make_shared<Tool<TSeq> >(t));
    prevalence_tool.push_back(preval);
    prevalence_tool_as_proportion.push_back(false);
    tools_dist_funs.push_back(nullptr);

}

template<typename TSeq>
inline void Model<TSeq>::add_tool_fun(Tool<TSeq> & t, ToolToAgentFun<TSeq> fun)
{
    
    db.record_tool(t);
    
    tools.push_back(std::make_shared<Tool<TSeq> >(t));
    prevalence_tool.push_back(0.0);
    prevalence_tool_as_proportion.push_back(false);
    tools_dist_funs.push_back(fun);
}


template<typename TSeq>
inline void Model<TSeq>::add_entity(Entity<TSeq> e)
{

    e.model = this;
    e.id = entities.size();
    entities.push_back(e);

}

template<typename TSeq>
inline void Model<TSeq>::rm_virus(size_t virus_pos)
{

    if (viruses.size() <= virus_pos)
        throw std::range_error(
            std::string("The specified virus (") +
            std::to_string(virus_pos) +
            std::string(") is out of range. ") +
            std::string("There are only ") +
            std::to_string(viruses.size()) +
            std::string(" viruses.")
            );

    // Flipping with the last one
    std::swap(viruses[virus_pos], viruses[viruses.size() - 1]);
    std::swap(viruses_dist_funs[virus_pos], viruses_dist_funs[viruses.size() - 1]);
    std::swap(prevalence_virus[virus_pos], prevalence_virus[viruses.size() - 1]);
    std::vector<bool>::swap(
        prevalence_virus_as_proportion[virus_pos],
        prevalence_virus_as_proportion[viruses.size() - 1]
        );

    viruses.pop_back();
    viruses_dist_funs.pop_back();
    prevalence_virus.pop_back();
    prevalence_virus_as_proportion.pop_back();

    return;

}

template<typename TSeq>
inline void Model<TSeq>::rm_tool(size_t tool_pos)
{

    if (tools.size() <= tool_pos)
        throw std::range_error(
            std::string("The specified tool (") +
            std::to_string(tool_pos) +
            std::string(") is out of range. ") +
            std::string("There are only ") +
            std::to_string(tools.size()) +
            std::string(" tools.")
            );

    // Flipping with the last one
    std::swap(tools[tool_pos], tools[tools.size() - 1]);
    std::swap(tools_dist_funs[tool_pos], tools_dist_funs[tools.size() - 1]);
    std::swap(prevalence_tool[tool_pos], prevalence_tool[tools.size() - 1]);
    
    /* There's an error on windows:
    https://github.com/UofUEpiBio/epiworldR/actions/runs/4801482395/jobs/8543744180#step:6:84

    More clear here:
    https://stackoverflow.com/questions/58660207/why-doesnt-stdswap-work-on-vectorbool-elements-under-clang-win
    */
    std::vector<bool>::swap(
        prevalence_tool_as_proportion[tool_pos],
        prevalence_tool_as_proportion[tools.size() - 1]
    );

    // auto old = prevalence_tool_as_proportion[tool_pos];
    // prevalence_tool_as_proportion[tool_pos] = prevalence_tool_as_proportion[tools.size() - 1];
    // prevalence_tool_as_proportion[tools.size() - 1] = old;
    

    tools.pop_back();
    tools_dist_funs.pop_back();
    prevalence_tool.pop_back();
    prevalence_tool_as_proportion.pop_back();

    return;

}

template<typename TSeq>
inline void Model<TSeq>::load_agents_entities_ties(
    std::string fn,
    int skip
    )
{

    int i,j;
    std::ifstream filei(fn);

    if (!filei)
        throw std::logic_error("The file " + fn + " was not found.");

    int linenum = 0;
    std::vector< epiworld_fast_uint > source_;
    std::vector< std::vector< epiworld_fast_uint > > target_(entities.size());

    target_.reserve(1e5);

    while (!filei.eof())
    {

        if (linenum++ < skip)
            continue;

        filei >> i >> j;

        // Looking for exceptions
        if (filei.bad())
            throw std::logic_error(
                "I/O error while reading the file " +
                fn
            );

        if (filei.fail())
            break;

        if (i >= static_cast<int>(this->size()))
            throw std::range_error(
                "The agent["+std::to_string(linenum)+"] = " + std::to_string(i) +
                " is above the max id " + std::to_string(this->size() - 1)
                );

        if (j >= static_cast<int>(this->entities.size()))
            throw std::range_error(
                "The entity["+std::to_string(linenum)+"] = " + std::to_string(j) +
                " is above the max id " + std::to_string(this->entities.size() - 1)
                );

        target_[j].push_back(i);

        population[i].add_entity(entities[j], nullptr);

    }

    // // Iterating over entities
    // for (size_t e = 0u; e < entities.size(); ++e)
    // {

    //     // This entity will have individuals assigned to it, so we add it
    //     if (target_[e].size() > 0u)
    //     {

    //         // Filling in the gaps
    //         prevalence_entity[e] = static_cast<epiworld_double>(target_[e].size());
    //         prevalence_entity_as_proportion[e] = false;

    //         // Generating the assignment function
    //         auto who = target_[e];
    //         entities_dist_funs[e] =
    //             [who](Entity<TSeq> & e, Model<TSeq>* m) -> void {

    //                 for (auto w : who)
    //                     m->population[w].add_entity(e, m, e.state_init, e.queue_init);
                    
    //                 return;
                    
    //             };

    //     }

    // }

    return;

}

template<typename TSeq>
inline void Model<TSeq>::agents_from_adjlist(
    std::string fn,
    int size,
    int skip,
    bool directed
    ) {

    AdjList al;
    al.read_edgelist(fn, size, skip, directed);
    this->agents_from_adjlist(al);

}

template<typename TSeq>
inline void Model<TSeq>::agents_from_edgelist(
    const std::vector< int > & source,
    const std::vector< int > & target,
    int size,
    bool directed
) {

    AdjList al(source, target, size, directed);
    agents_from_adjlist(al);

}

template<typename TSeq>
inline void Model<TSeq>::agents_from_adjlist(AdjList al) {

    // Resizing the people
    agents_empty_graph(al.vcount());
    
    const auto & tmpdat = al.get_dat();
    
    for (size_t i = 0u; i < tmpdat.size(); ++i)
    {

        // population[i].id    = i;
        population[i].model = this;

        for (const auto & link: tmpdat[i])
        {

            population[i].add_neighbor(
                population[link.first],
                true, true
                );

        }

    }

    #ifdef EPI_DEBUG
    for (auto & p: population)
    {
        if (p.id >= static_cast<int>(al.vcount()))
            throw std::logic_error(
                "Agent's id cannot be negative above or equal to the number of agents!");
    }
    #endif

}

template<typename TSeq>
inline bool Model<TSeq>::is_directed() const
{
    if (population.size() == 0u)
        throw std::logic_error("The population hasn't been initialized.");

    return directed;
}

template<typename TSeq>
inline int Model<TSeq>::today() const {

    if (ndays == 0)
      return 0;

    return this->current_date;
}

template<typename TSeq>
inline void Model<TSeq>::next() {

    db.record();
    ++this->current_date;
    
    // Advancing the progress bar
    if ((this->current_date >= 1) && verbose)
        pb.next();

    return ;
}

template<typename TSeq>
inline void Model<TSeq>::run(
    epiworld_fast_uint ndays,
    int seed
) 
{

    if (size() == 0u)
        throw std::logic_error("There's no agents in this model!");

    if (nstates == 0u)
        throw std::logic_error(
            std::string("No states registered in this model. ") +
            std::string("At least one state should be included. See the function -Model::add_state()-")
            );

    // Setting up the number of steps
    this->ndays = ndays;

    if (seed >= 0)
        engine.seed(seed);

    array_double_tmp.resize(std::max(
        size(),
        static_cast<size_t>(1024 * 1024)
    ));


    array_virus_tmp.resize(1024);

    // Checking whether the proposed state in/out/removed
    // are valid
    epiworld_fast_int _init, _end, _removed;
    int nstate_int = static_cast<int>(nstates);
    for (auto & v : viruses)
    {
        v->get_state(&_init, &_end, &_removed);
        
        // Negative unspecified state
        if (((_init != -99) && (_init < 0)) || (_init >= nstate_int))
            throw std::range_error("States must be between 0 and " +
                std::to_string(nstates - 1));

        // Negative unspecified state
        if (((_end != -99) && (_end < 0)) || (_end >= nstate_int))
            throw std::range_error("States must be between 0 and " +
                std::to_string(nstates - 1));

        if (((_removed != -99) && (_removed < 0)) || (_removed >= nstate_int))
            throw std::range_error("States must be between 0 and " +
                std::to_string(nstates - 1));

    }

    for (auto & t : tools)
    {
        t->get_state(&_init, &_end);
        
        // Negative unspecified state
        if (((_init != -99) && (_init < 0)) || (_init >= nstate_int))
            throw std::range_error("States must be between 0 and " +
                std::to_string(nstates - 1));

        // Negative unspecified state
        if (((_end != -99) && (_end < 0)) || (_end >= nstate_int))
            throw std::range_error("States must be between 0 and " +
                std::to_string(nstates - 1));

    }

    // Starting first infection and tools
    reset();

    // Initializing the simulation
    chrono_start();
    EPIWORLD_RUN((*this))
    {

        #ifdef EPI_DEBUG
        db.n_transmissions_potential = 0;
        db.n_transmissions_today = 0;
        #endif

        // We can execute these components in whatever order the
        // user needs.
        this->update_state();
    
        // We start with the global actions
        this->run_global_actions();

        // In this case we are applying degree sequence rewiring
        // to change the network just a bit.
        this->rewire();

        // This locks all the changes
        this->next();

        // Mutation must happen at the very end of all
        this->mutate_virus();

    }

    // The last reaches the end...
    this->current_date--;

    chrono_end();

}

template<typename TSeq>
inline void Model<TSeq>::run_multiple(
    epiworld_fast_uint ndays,
    epiworld_fast_uint nexperiments,
    int seed_,
    std::function<void(size_t,Model<TSeq>*)> fun,
    bool reset,
    bool verbose,
    int nthreads
)
{

    if (seed_ >= 0)
        this->seed(seed_);

    // Seeds will be reproducible by default
    std::vector< int > seeds_n(nexperiments);
    // #ifdef EPI_DEBUG
    // std::fill(
    //     seeds_n.begin(),
    //     seeds_n.end(),
    //     std::floor(
    //         runif() * static_cast<double>(std::numeric_limits<int>::max())
    //     )
    //     );
    // #else
    for (auto & s : seeds_n)
    {
        s = static_cast<int>(
            std::floor(
                runif() * static_cast<double>(std::numeric_limits<int>::max())
                )
        );
    }
    // #endif

    EPI_DEBUG_NOTIFY_ACTIVE()

    bool old_verb = this->verbose;
    verbose_off();

    // Setting up backup
    if (reset)
        set_backup();

    #ifdef _OPENMP

    omp_set_num_threads(nthreads);

    // Generating copies of the model
    std::vector< Model<TSeq> * > these;
    for (size_t i = 0; i < static_cast<size_t>(std::max(nthreads - 1, 0)); ++i)
        these.push_back(clone_ptr());

    // Figuring out how many replicates
    std::vector< size_t > nreplicates(nthreads, 0);
    std::vector< size_t > nreplicates_csum(nthreads, 0);
    size_t sums = 0u;
    for (int i = 0; i < nthreads; ++i)
    {
        nreplicates[i] = static_cast<epiworld_fast_uint>(
            std::floor(nexperiments/nthreads)
            );
        
        // This takes the cumsum
        nreplicates_csum[i] = sums;

        sums += nreplicates[i];

    }

    if (sums < nexperiments)
        nreplicates[nthreads - 1] += (nexperiments - sums);

    Progress pb_multiple(
        nreplicates[0u],
        EPIWORLD_PROGRESS_BAR_WIDTH
        );

    if (verbose)
    {

        printf_epiworld(
            "Starting multiple runs (%i) using %i thread(s)\n", 
            static_cast<int>(nexperiments),
            static_cast<int>(nthreads)
        );

        pb_multiple.start();

    }

    #ifdef EPI_DEBUG
    // Checking the initial state of all the models. Throw an
    // exception if they are not the same.
    for (size_t i = 1; i < static_cast<size_t>(std::max(nthreads - 1, 0)); ++i)
    {

        if (db != these[i]->db)
        {
            throw std::runtime_error(
                "The initial state of the models is not the same"
            );
        }
    }
    #endif

    #pragma omp parallel shared(these, nreplicates, nreplicates_csum, seeds_n) \
        firstprivate(nexperiments, nthreads, fun, reset, verbose, pb_multiple, ndays) \
        default(shared)
    {

        auto iam = omp_get_thread_num();

        for (size_t n = 0u; n < nreplicates[iam]; ++n)
        {
            size_t sim_id = nreplicates_csum[iam] + n;
            if (iam == 0)
            {

                // Initializing the seed
                run(ndays, seeds_n[sim_id]);

                if (fun)
                    fun(n, this);

                // Only the first one prints
                if (verbose)
                    pb_multiple.next();

            } else {

                // Initializing the seed
                these[iam - 1]->run(ndays, seeds_n[sim_id]);

                if (fun)
                    fun(sim_id, these[iam - 1]);

            }

        }
        
    }

    // Adjusting the number of replicates
    n_replicates += (nexperiments - nreplicates[0u]);

    for (auto & ptr : these)
        delete ptr;

    #else
    // if (reset)
    //     set_backup();

    Progress pb_multiple(
        nexperiments,
        EPIWORLD_PROGRESS_BAR_WIDTH
        )
        ;
    if (verbose)
    {

        printf_epiworld(
            "Starting multiple runs (%i)\n", 
            static_cast<int>(nexperiments)
        );

        pb_multiple.start();

    }

    for (size_t n = 0u; n < nexperiments; ++n)
    {

        run(ndays, seeds_n[n]);

        if (fun)
            fun(n, this);

        if (verbose)
            pb_multiple.next();
    
    }
    #endif

    if (verbose)
        pb_multiple.end();

    if (old_verb)
        verbose_on();

    return;

}

template<typename TSeq>
inline void Model<TSeq>::update_state() {

    // Next state
    if (use_queuing)
    {
        int i = -1;
        for (auto & p: population)
            if (queue[++i] > 0)
            {
                if (state_fun[p.state])
                    state_fun[p.state](&p, this);
            }

    }
    else
    {

        for (auto & p: population)
            if (state_fun[p.state])
                    state_fun[p.state](&p, this);

    }

    actions_run();
    
}



template<typename TSeq>
inline void Model<TSeq>::mutate_virus() {

    if (use_queuing)
    {

        int i = -1;
        for (auto & p: population)
        {

            if (queue[++i] == 0)
                continue;

            if (p.n_viruses > 0u)
                for (auto & v : p.get_viruses())
                    v->mutate(this);

        }

    }
    else 
    {

        for (auto & p: population)
        {

            if (p.n_viruses > 0u)
                for (auto & v : p.get_viruses())
                    v->mutate(this);

        }

    }
    

}

template<typename TSeq>
inline size_t Model<TSeq>::get_n_viruses() const {
    return db.size();
}

template<typename TSeq>
inline size_t Model<TSeq>::get_n_tools() const {
    return tools.size();
}

template<typename TSeq>
inline epiworld_fast_uint Model<TSeq>::get_ndays() const {
    return ndays;
}

template<typename TSeq>
inline epiworld_fast_uint Model<TSeq>::get_n_replicates() const
{
    return n_replicates;
}

template<typename TSeq>
inline void Model<TSeq>::set_ndays(epiworld_fast_uint ndays) {
    this->ndays = ndays;
}

template<typename TSeq>
inline bool Model<TSeq>::get_verbose() const {
    return verbose;
}

template<typename TSeq>
inline void Model<TSeq>::verbose_on() {
    verbose = true;
}

template<typename TSeq>
inline void Model<TSeq>::verbose_off() {
    verbose = false;
}

template<typename TSeq>
inline void Model<TSeq>::set_rewire_fun(
    std::function<void(std::vector<Agent<TSeq>>*,Model<TSeq>*,epiworld_double)> fun
    ) {
    rewire_fun = fun;
}

template<typename TSeq>
inline void Model<TSeq>::set_rewire_prop(epiworld_double prop)
{

    if (prop < 0.0)
        throw std::range_error("Proportions cannot be negative.");

    if (prop > 1.0)
        throw std::range_error("Proportions cannot be above 1.0.");

    rewire_prop = prop;
}

template<typename TSeq>
inline epiworld_double Model<TSeq>::get_rewire_prop() const {
    return rewire_prop;
}

template<typename TSeq>
inline void Model<TSeq>::rewire() {

    if (rewire_fun)
        rewire_fun(&population, this, rewire_prop);
}


template<typename TSeq>
inline void Model<TSeq>::write_data(
    std::string fn_virus_info,
    std::string fn_virus_hist,
    std::string fn_tool_info,
    std::string fn_tool_hist,
    std::string fn_total_hist,
    std::string fn_transmission,
    std::string fn_transition,
    std::string fn_reproductive_number,
    std::string fn_generation_time
    ) const
{

    db.write_data(
        fn_virus_info, fn_virus_hist,
        fn_tool_info, fn_tool_hist,
        fn_total_hist, fn_transmission, fn_transition,
        fn_reproductive_number, fn_generation_time
        );

}

template<typename TSeq>
inline void Model<TSeq>::write_edgelist(
    std::string fn
    ) const
{

    // Figuring out the writing sequence
    std::vector< const Agent<TSeq> * > wseq(size());
    for (const auto & p: population)
        wseq[p.id] = &p;

    std::ofstream efile(fn, std::ios_base::out);
    efile << "source target\n";
    if (this->is_directed())
    {

        for (const auto & p : wseq)
        {
            for (auto & n : p->neighbors)
                efile << p->id << " " << n << "\n";
        }

    } else {

        for (const auto & p : wseq)
        {
            for (auto & n : p->neighbors)
                if (static_cast<int>(p->id) <= static_cast<int>(n))
                    efile << p->id << " " << n << "\n";
        }

    }

}

template<typename TSeq>
inline void Model<TSeq>::write_edgelist(
std::vector< int > & source,
std::vector< int > & target
) const {

    // Figuring out the writing sequence
    std::vector< const Agent<TSeq> * > wseq(size());
    for (const auto & p: population)
        wseq[p.id] = &p;

    if (this->is_directed())
    {

        for (const auto & p : wseq)
        {
            for (auto & n : p->neighbors)
            {
                source.push_back(static_cast<int>(p->id));
                target.push_back(static_cast<int>(n));
            }
        }

    } else {

        for (const auto & p : wseq)
        {
            for (auto & n : p->neighbors) {
                if (static_cast<int>(p->id) <= static_cast<int>(n)) {
                    source.push_back(static_cast<int>(p->id));
                    target.push_back(static_cast<int>(n));
                }
            }
        }

    }


}

template<typename TSeq>
inline std::map<std::string,epiworld_double> & Model<TSeq>::params()
{
    return parameters;
}

template<typename TSeq>
inline void Model<TSeq>::reset() {

    // Restablishing people
    pb = Progress(ndays, 80);

    if (population_backup.size() != 0u)
    {
        population = population_backup;

        #ifdef EPI_DEBUG
        for (size_t i = 0; i < population.size(); ++i)
        {

            if (population[i] != population_backup[i])
                throw std::logic_error("Model::reset population doesn't match.");

        }
        #endif

    } 
    else
    {
        for (auto & p : population)
            p.reset();

        #ifdef EPI_DEBUG
        for (auto & a: population)
        {
            if (a.get_state() != 0u)
                throw std::logic_error("Model::reset population doesn't match."
                    "Some agents are not in the baseline state.");
        }
        #endif
    }
        
    if (entities_backup.size() != 0)
    {
        entities = entities_backup;

        #ifdef EPI_DEBUG
        for (size_t i = 0; i < entities.size(); ++i)
        {

            if (entities[i] != entities_backup[i])
                throw std::logic_error("Model::reset entities don't match.");

        }
        #endif
        
    }
    else
    {
        for (auto & e: entities)
            e.reset();
    }
    
    current_date = 0;

    db.reset();

    // This also clears the queue
    if (use_queuing)
        queue.reset();

    // Re distributing tools and virus
    dist_virus();
    dist_tools();

    // Recording the original state (at time 0) and advancing
    // to time 1
    next();


}

// Too big to keep here
#include "model-meat-print.hpp"

template<typename TSeq>
inline Model<TSeq> && Model<TSeq>::clone() const {

    // Step 1: Regen the individuals and make sure that:
    //  - Neighbors point to the right place
    //  - DB is pointing to the right place
    Model<TSeq> res(*this);

    // Removing old neighbors
    for (auto & p: res.population)
        p.neighbors.clear();
    
    // Rechecking individuals
    for (epiworld_fast_uint p = 0u; p < size(); ++p)
    {
        // Making room
        const Agent<TSeq> & agent_this = population[p];
        Agent<TSeq> & agent_res  = res.population[p];

        // Agent pointing to the right model and agent
        agent_res.model         = &res;
        agent_res.viruses.agent = &agent_res;
        agent_res.tools.agent   = &agent_res;

        // Readding
        std::vector< Agent<TSeq> * > neigh = agent_this.neighbors;
        for (epiworld_fast_uint n = 0u; n < neigh.size(); ++n)
        {
            // Point to the right neighbors
            int loc = res.population_ids[neigh[n]->get_id()];
            agent_res.add_neighbor(res.population[loc], true, true);

        }

    }

    return res;

}



template<typename TSeq>
inline void Model<TSeq>::add_state(
    std::string lab, 
    UpdateFun<TSeq> fun
)
{

    // Checking it doesn't match
    for (auto & s : states_labels)
        if (s == lab)
            throw std::logic_error("state \"" + s + "\" already registered.");

    states_labels.push_back(lab);
    state_fun.push_back(fun);
    nstates++;

}


template<typename TSeq>
inline const std::vector< std::string > &
Model<TSeq>::get_states() const
{
    return states_labels;
}

template<typename TSeq>
inline const std::vector< UpdateFun<TSeq> > &
Model<TSeq>::get_state_fun() const
{
    return state_fun;
}

template<typename TSeq>
inline void Model<TSeq>::print_state_codes() const
{

    // Horizontal line
    std::string line = "";
    for (epiworld_fast_uint i = 0u; i < 80u; ++i)
        line += "_";

    printf_epiworld("\n%s\nstates CODES\n\n", line.c_str());

    epiworld_fast_uint nchar = 0u;
    for (auto & p : states_labels)
        if (p.length() > nchar)
            nchar = p.length();
    
    std::string fmt = " %2i = %-" + std::to_string(nchar + 1 + 4) + "s\n";
    for (epiworld_fast_uint i = 0u; i < nstates; ++i)
    {

        printf_epiworld(
            fmt.c_str(),
            i,
            (states_labels[i] + " (S)").c_str()
        );

    }

}



template<typename TSeq>
inline epiworld_double Model<TSeq>::add_param(
    epiworld_double initial_value,
    std::string pname
    ) {

    if (parameters.find(pname) == parameters.end())
        parameters[pname] = initial_value;
    
    return initial_value;

}

template<typename TSeq>
inline void Model<TSeq>::read_params(std::string fn)
{

    std::ifstream paramsfile(fn);

    if (!paramsfile)
        throw std::logic_error("The file " + fn + " was not found.");

    std::regex pattern("^([^:]+)\\s*[:]\\s*([0-9]+|[0-9]*\\.[0-9]+)?\\s*$");

    std::string line;
    std::smatch match;
    auto empty = std::sregex_iterator();

    while (std::getline(paramsfile, line))
    {

        // Is it a comment or an empty line?
        if (std::regex_match(line, std::regex("^([*].+|//.+|#.+|\\s*)$")))
            continue;

        // Finding the patter, if it doesn't match, then error
        std::regex_match(line, match, pattern);

        if (match.empty())
            throw std::logic_error("The line does not match parameters:\n" + line);

        // Capturing the number
        std::string anumber = match[2u].str() + match[3u].str();
        epiworld_double tmp_num = static_cast<epiworld_double>(
            std::strtod(anumber.c_str(), nullptr)
            );

        add_param(
            tmp_num,
            std::regex_replace(
                match[1u].str(),
                std::regex("^\\s+|\\s+$"),
                "")
        );

    }

}

template<typename TSeq>
inline epiworld_double Model<TSeq>::get_param(std::string pname)
{
    if (parameters.find(pname) == parameters.end())
        throw std::logic_error("The parameter " + pname + " does not exists.");

    return parameters[pname];
}

template<typename TSeq>
inline void Model<TSeq>::set_param(std::string pname, epiworld_double value)
{
    if (parameters.find(pname) == parameters.end())
        throw std::logic_error("The parameter " + pname + " does not exists.");

    parameters[pname] = value;

    return;

}

// // Same as before but using the size_t method
// template<typename TSeq>
// inline void Model<TSeq>::set_param(size_t k, epiworld_double value)
// {
//     if (k >= parameters.size())
//         throw std::logic_error("The parameter index " + std::to_string(k) + " does not exists.");

//     // Access the k-th element of the std::unordered_map parameters


//     *(parameters.begin() + k) = value;

//     return;
// }

template<typename TSeq>
inline epiworld_double Model<TSeq>::par(std::string pname)
{
    return parameters[pname];
}

#define DURCAST(tunit,txtunit) {\
        elapsed       = std::chrono::duration_cast<std::chrono:: tunit>(\
            time_end - time_start).count(); \
        elapsed_total = std::chrono::duration_cast<std::chrono:: tunit>(time_elapsed).count(); \
        abbr_unit     = txtunit;}

template<typename TSeq>
inline void Model<TSeq>::get_elapsed(
    std::string unit,
    epiworld_double * last_elapsed,
    epiworld_double * total_elapsed,
    std::string * unit_abbr,
    bool print
) const {

    // Preparing the result
    epiworld_double elapsed, elapsed_total;
    std::string abbr_unit;

    // Figuring out the length
    if (unit == "auto")
    {

        size_t tlength = std::to_string(
            static_cast<int>(floor(time_elapsed.count()))
            ).length();
        
        if (tlength <= 1)
            unit = "nanoseconds";
        else if (tlength <= 3)
            unit = "microseconds";
        else if (tlength <= 6)
            unit = "milliseconds";
        else if (tlength <= 8)
            unit = "seconds";
        else if (tlength <= 9)
            unit = "minutes";
        else 
            unit = "hours";

    }

    if (unit == "nanoseconds")       DURCAST(nanoseconds,"ns")
    else if (unit == "microseconds") DURCAST(microseconds,"\xC2\xB5s")
    else if (unit == "milliseconds") DURCAST(milliseconds,"ms")
    else if (unit == "seconds")      DURCAST(seconds,"s")
    else if (unit == "minutes")      DURCAST(minutes,"m")
    else if (unit == "hours")        DURCAST(hours,"h")
    else
        throw std::range_error("The time unit " + unit + " is not supported.");


    if (last_elapsed != nullptr)
        *last_elapsed = elapsed;
    if (total_elapsed != nullptr)
        *total_elapsed = elapsed_total;
    if (unit_abbr != nullptr)
        *unit_abbr = abbr_unit;

    if (!print)
        return;

    if (n_replicates > 1u)
    {
        printf_epiworld("last run elapsed time : %.2f%s\n",
            elapsed, abbr_unit.c_str());
        printf_epiworld("total elapsed time    : %.2f%s\n",
            elapsed_total, abbr_unit.c_str());
        printf_epiworld("total runs            : %i\n",
            static_cast<int>(n_replicates));
        printf_epiworld("mean run elapsed time : %.2f%s\n",
            elapsed_total/static_cast<epiworld_double>(n_replicates), abbr_unit.c_str());

    } else {
        printf_epiworld("last run elapsed time : %.2f%s.\n", elapsed, abbr_unit.c_str());
    }
}

template<typename TSeq>
inline void Model<TSeq>::set_user_data(std::vector< std::string > names)
{
    db.set_user_data(names);
}

template<typename TSeq>
inline void Model<TSeq>::add_user_data(epiworld_fast_uint j, epiworld_double x)
{
    db.add_user_data(j, x);
}

template<typename TSeq>
inline void Model<TSeq>::add_user_data(std::vector<epiworld_double> x)
{
    db.add_user_data(x);
}

template<typename TSeq>
inline UserData<TSeq> & Model<TSeq>::get_user_data()
{
    return db.get_user_data();
}

template<typename TSeq>
inline void Model<TSeq>::add_global_action(
    std::function<void(Model<TSeq>*)> fun,
    std::string name,
    int date
)
{

    global_actions.push_back(
        GlobalAction<TSeq>(
            fun,
            name,
            date
            )
    );

}

template<typename TSeq>
inline void Model<TSeq>::add_global_action(
    GlobalAction<TSeq> action
)
{
    global_actions.push_back(action);
}

template<typename TSeq>
GlobalAction<TSeq> & Model<TSeq>::get_global_action(
    std::string name
)
{

    for (auto & a : global_actions)
        if (a.name == name)
            return a;

    throw std::logic_error("The global action " + name + " was not found.");

}

template<typename TSeq>
GlobalAction<TSeq> & Model<TSeq>::get_global_action(
    size_t index
)
{

    if (index >= global_actions.size())
        throw std::range_error("The index " + std::to_string(index) + " is out of range.");

    return global_actions[index];

}

// Remove implementation
template<typename TSeq>
inline void Model<TSeq>::rm_global_action(
    std::string name
)
{

    for (auto it = global_actions.begin(); it != global_actions.end(); ++it)
    {
        if (it->get_name() == name)
        {
            global_actions.erase(it);
            return;
        }
    }

    throw std::logic_error("The global action " + name + " was not found.");

}

// Same as above, but the index implementation
template<typename TSeq>
inline void Model<TSeq>::rm_global_action(
    size_t index
)
{

    if (index >= global_actions.size())
        throw std::range_error("The index " + std::to_string(index) + " is out of range.");

    global_actions.erase(global_actions.begin() + index);

}

template<typename TSeq>
inline void Model<TSeq>::run_global_actions()
{

    for (auto & action: global_actions)
    {
        action(this, today());
        actions_run();
    }

}

template<typename TSeq>
inline void Model<TSeq>::queuing_on()
{
    use_queuing = true;
}

template<typename TSeq>
inline void Model<TSeq>::queuing_off()
{
    use_queuing = false;
}

template<typename TSeq>
inline bool Model<TSeq>::is_queuing_on() const
{
    return use_queuing;
}

template<typename TSeq>
inline Queue<TSeq> & Model<TSeq>::get_queue()
{
    return queue;
}

template<typename TSeq>
inline const std::vector< VirusPtr<TSeq> > & Model<TSeq>::get_viruses() const
{
    return viruses;
}

template<typename TSeq>
const std::vector< ToolPtr<TSeq> > & Model<TSeq>::get_tools() const
{
    return tools;
}

template<typename TSeq>
inline Virus<TSeq> & Model<TSeq>::get_virus(size_t id)
{

    if (viruses.size() <= id)
        throw std::length_error("The specified id for the virus is out of range");

    return *viruses[id];

}

template<typename TSeq>
inline Tool<TSeq> & Model<TSeq>::get_tool(size_t id)
{

    if (tools.size() <= id)
        throw std::length_error("The specified id for the tools is out of range");

    return *tools[id];

}


template<typename TSeq>
inline void Model<TSeq>::set_agents_data(double * data_, size_t ncols_)
{
    agents_data = data_;
    agents_data_ncols = ncols_;
}

template<typename TSeq>
inline double * Model<TSeq>::get_agents_data() {
    return this->agents_data;
}

template<typename TSeq>
inline size_t Model<TSeq>::get_agents_data_ncols() const {
    return this->agents_data_ncols;
}


template<typename TSeq>
inline void Model<TSeq>::set_name(std::string name)
{
    this->name = name;
}

template<typename TSeq>
inline std::string Model<TSeq>::get_name() const 
{
    return this->name;
}

#define VECT_MATCH(a, b, c) \
    EPI_DEBUG_FAIL_AT_TRUE(a.size() != b.size(), c) \
    for (size_t __i = 0u; __i < a.size(); ++__i) \
    {\
        EPI_DEBUG_FAIL_AT_TRUE(a[__i] != b[__i], c) \
    }

template<typename TSeq>
inline bool Model<TSeq>::operator==(const Model<TSeq> & other) const
{
    EPI_DEBUG_FAIL_AT_TRUE(name != other.name, "names don't match")
    EPI_DEBUG_FAIL_AT_TRUE(db != other.db, "database don't match")

    VECT_MATCH(population, other.population, "population doesn't match")

    EPI_DEBUG_FAIL_AT_TRUE(
        using_backup != other.using_backup,
        "Model:: using_backup don't match"
        )
    
    if ((population_backup.size() != 0) & (other.population_backup.size() != 0))
    {

        // False is population_backup.size() != other.population_backup.size()
        if (population_backup.size() != other.population_backup.size())
            return false;

        for (size_t i = 0u; i < population_backup.size(); ++i)
        {
            if (population_backup[i] != other.population_backup[i])
                return false;
        }
        
    } else if ((population_backup.size() == 0) & (other.population_backup.size() != 0)) {
        return false;
    } else if ((population_backup.size() != 0) & (other.population_backup.size() == 0))
    {
        return false;
    }

    EPI_DEBUG_FAIL_AT_TRUE(
        agents_data != other.agents_data,
        "Model:: agents_data don't match"
    )

    EPI_DEBUG_FAIL_AT_TRUE(
        agents_data_ncols != other.agents_data_ncols,
        "Model:: agents_data_ncols don't match"
    )

    EPI_DEBUG_FAIL_AT_TRUE(
        directed != other.directed,
        "Model:: directed don't match"
    )
    
    // Viruses -----------------------------------------------------------------
    EPI_DEBUG_FAIL_AT_TRUE(
        viruses.size() != other.viruses.size(),
        "Model:: viruses.size() don't match"
        )

    for (size_t i = 0u; i < viruses.size(); ++i)
    {
        EPI_DEBUG_FAIL_AT_TRUE(
            *viruses[i] != *other.viruses[i],
            "Model:: *viruses[i] don't match"
        )
            
    }

    VECT_MATCH(
        prevalence_virus,
        other.prevalence_virus,
        "virus prevalence don't match"
    )

    VECT_MATCH(
        prevalence_virus_as_proportion,
        other.prevalence_virus_as_proportion,
        "virus prevalence as prop don't match"
    )
    
    // Tools -------------------------------------------------------------------
    EPI_DEBUG_FAIL_AT_TRUE(
        tools.size() != other.tools.size(),
        "Model:: tools.size() don't match"
        )
        
    for (size_t i = 0u; i < tools.size(); ++i)
    {
        EPI_DEBUG_FAIL_AT_TRUE(
            *tools[i] != *other.tools[i],
            "Model:: *tools[i] don't match"
        )
            
    }

    VECT_MATCH(
        prevalence_tool, 
        other.prevalence_tool, 
        "tools prevalence don't match"
    )

    VECT_MATCH(
        prevalence_tool_as_proportion, 
        other.prevalence_tool_as_proportion, 
        "tools as prop don't match"
    )
    
    VECT_MATCH(
        entities,
        other.entities,
        "entities don't match"
    )

    if ((entities_backup.size() != 0) & (other.entities_backup.size() != 0))
    {
        
        for (size_t i = 0u; i < entities_backup.size(); ++i)
        {

            EPI_DEBUG_FAIL_AT_TRUE(
                entities_backup[i] != other.entities_backup[i],
                "Model:: entities_backup[i] don't match"
            )

        }
        
    } else if ((entities_backup.size() == 0) & (other.entities_backup.size() != 0)) {
        EPI_DEBUG_FAIL_AT_TRUE(true, "entities_backup don't match")
    } else if ((entities_backup.size() != 0) & (other.entities_backup.size() == 0))
    {
        EPI_DEBUG_FAIL_AT_TRUE(true, "entities_backup don't match")
    }

    EPI_DEBUG_FAIL_AT_TRUE(
        rewire_prop != other.rewire_prop,
        "Model:: rewire_prop don't match"
    )
        
    EPI_DEBUG_FAIL_AT_TRUE(
        parameters.size() != other.parameters.size(),
        "Model:: () don't match"
    )

    EPI_DEBUG_FAIL_AT_TRUE(
        parameters != other.parameters,
        "Model:: parameters don't match"
    )

    EPI_DEBUG_FAIL_AT_TRUE(
        ndays != other.ndays,
        "Model:: ndays don't match"
    )
    
    VECT_MATCH(
        states_labels,
        other.states_labels,
        "state labels don't match"
    )

    EPI_DEBUG_FAIL_AT_TRUE(
        nstates != other.nstates,
        "Model:: nstates don't match"
    )
    
    EPI_DEBUG_FAIL_AT_TRUE(
        verbose != other.verbose,
        "Model:: verbose don't match"
    )

    EPI_DEBUG_FAIL_AT_TRUE(
        current_date != other.current_date,
        "Model:: current_date don't match"
    )

    VECT_MATCH(global_actions, other.global_actions, "global action don't match");

    EPI_DEBUG_FAIL_AT_TRUE(
        queue != other.queue,
        "Model:: queue don't match"
    )
    

    EPI_DEBUG_FAIL_AT_TRUE(
        use_queuing != other.use_queuing,
        "Model:: use_queuing don't match"
    )
    
    return true;

}

#undef VECT_MATCH
#undef DURCAST
#undef CASES_PAR
#undef CASE_PAR
#undef CHECK_INIT
#endif

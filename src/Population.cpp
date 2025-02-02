#include <random>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <cmath>  //para el valor absoluto abs

#include "message.h"       
#include "parsimu.h"
#include "real.h"
#include "tuple_value.h"

#include "Population.h"           

using namespace std;


#define VERBOSE true

#define PRINT_TIMES(f) {\
	VTime timeleft = nextChange();\
	VTime elapsed  = msg.time() - lastChange();\
	VTime sigma    = elapsed + timeleft;\
	cout << f << "@" << msg.time() <<\
		" - timeleft: " << timeleft <<\
		" - elapsed: " << elapsed <<\
		" - sigma: " << sigma << endl;\
}

Population::Population( const string &name ) : 
	Atomic( name ),
	out(addOutputPort( "out" )),
	in(addInputPort( "in" )),
	population_id(500), //asumimos que no vamos a tener mas de 500 poblaciones? 
	age(100),
	university_studies(1),
	political_involvement(1),
	employment_status(1),
	economic_status(1),
	centrality(1),
	political_affinity(1),
	delay(1,999), // milisegundos
	rng(random_device()()),
	is_message_received_from_media(false),
	dist_float(0.0,1.0)
{
	population_id = str2Real( ParallelMainSimulator::Instance().getParameter( description(), "population_id" ) );
	age = str2Real( ParallelMainSimulator::Instance().getParameter( description(), "age" ) );
	university_studies = str2Real( ParallelMainSimulator::Instance().getParameter( description(), "university_studies" ) );
	political_involvement = str2Real( ParallelMainSimulator::Instance().getParameter( description(), "political_involvement" ) );
	employment_status = str2Real( ParallelMainSimulator::Instance().getParameter( description(), "employment_status" ) );
	economic_status = str2Real( ParallelMainSimulator::Instance().getParameter( description(), "economic_status" ) );
	centrality = str2Real( ParallelMainSimulator::Instance().getParameter( description(), "centrality" ) );
	political_affinity = str2Real( ParallelMainSimulator::Instance().getParameter( description(), "political_affinity" ) ); //this->dist_float(this->rng);
	// is_message_received_from_media = false;
}

Model &Population::initFunction()
{
	// [(!) Initialize common variables]
	this->elapsed  = VTime::Zero;
 	this->timeLeft = VTime::Inf;

	// seteamos un delay para que escupa el output inicial
    int delay = this->delay(this->rng);
 	this->sigma = VTime(0,0,0,delay);
 	
	holdIn( AtomicState::active, this->sigma );
	return *this ;
}

Model &Population::externalFunction( const ExternalMessage &msg )
{
#if VERBOSE
	PRINT_TIMES("dext");
#endif
		
	Tuple<Real> message = Tuple<Real>::from_value(msg.value());

	// si no es un mensaje a ignorar, encolamos la fake new.
	if (!(message.size() == 2 || // output "estadistico" de otra poblacion
		(message.size() > 11 && message[11] == population_id) // el mensaje es de la misma poblacion, c++ tiene evaluacion por cortocircuito asi que no explota :) 
	)){ 
        message_queue.push(message);
		/* Nos agendamos una transicion interna con cierto delay.
		   Nota: esto puede llegar a "pisar" un delay previo si el atomico se encontraba
		   procesando una fake new anterior, pero no importa.
		 */
		int delay = this->delay(this->rng);
		holdIn( AtomicState::active, VTime(0,0,0,delay));
    }

	return *this ;

}

Model &Population::internalFunction( const InternalMessage &msg )
{
#if VERBOSE
	PRINT_TIMES("dint");
#endif
	if(message_queue.empty()){
		// stays in passive state until an external event occurs;
		passivate();
	} else {
		// Generamos una transición interna con delay para enviar el mensaje a las redes
		auto delay = this->delay(this->rng);
		holdIn( AtomicState::active, VTime(0,0,0,delay) );
	}
	return *this;
}

Model &Population::outputFunction( const CollectMessage &msg )
{
	if(!message_queue.empty()){
		Tuple<Real> message = message_queue.front();
		message_queue.pop();

		Real attacked_party = message[0];
		current_fake_attacked_party = message[0];
		current_fake_urgency = message[1];
		current_fake_credibility = message[2];
		current_fake_media_party = message[3];

		current_fake_belief = message.size() == 4 ? this->beliefInFakeFromMedia(message) : this->beliefInFakeFromPopulation(message);

		int multiplicative_factor = attacked_party == 1 ? 1 : (-1); // para saber si restar o sumar -> acercarse al partido 0 o 1
		political_affinity = political_affinity + multiplicative_factor * current_fake_belief.value() * 0.0175; // con el 0.01 nos acercamos de a poco 
	
		if (political_affinity > 1) {
			political_affinity = 1;
		} else if (political_affinity < 0) {
			political_affinity = 0;
		}

		is_message_received_from_media = message.size() == 4 ;
	}

	if(is_message_received_from_media) {
		// Recibimos de Media, reenviamos a las otras poblaciones

		Tuple<Real> out_value{  current_fake_attacked_party,
					current_fake_urgency,
					current_fake_credibility,
					current_fake_media_party,
					age,
					university_studies,
					political_involvement,
					employment_status,
					economic_status,
					centrality,
					current_fake_belief,
					population_id,
					political_affinity
				};
		sendOutput( msg.time(), out, out_value ) ;
	} else {

		// mandamos output "estadistico" (las otras poblaciones la van a descartar, ver externalFunction)
		Tuple<Real> statistics{
			population_id,	
			political_affinity
			};
							
		sendOutput( msg.time(), out, statistics ) ;
	}
    

	return *this ;
}

Real Population::beliefInFakeFromMedia( Tuple<Real> message )
{
		Real attacked_party = message[0];
		Real urgency = message[1];
		Real credibility = message[2];
		Real media_party = message[3];

		Real belief =    (Real(1) - abs( (media_party - political_affinity).value() ) ) * 0.175   + // cuanto mas valga abs es que mas diferencia de afinidades hay - > menos va a creer
					(Real(1) - university_studies) * 0.125 +
					(Real(1) - abs( (economic_status - 0.5).value() )) * 0.125 + // 0.5 porque si esta en un extremo va a tener de esta forma un valor mayor
					(Real(1) - employment_status) * 0.125 +
					(Real(1) - political_involvement) * 0.125 +
					abs( (attacked_party - political_affinity).value() ) * 0.175 +
					urgency * 0.1 +
					credibility * 0.05;
		
		return belief; 

}

Real Population::beliefInFakeFromPopulation( Tuple<Real> message )
{
		Real attacked_party = message[0];
		Real urgency = message[1];
		Real credibility = message[2];
		Real media_party = message[3]; 
		Real sender_age = message[4];
		Real sender_university_studies = message[5];
		Real sender_political_involvement = message[6];
		Real sender_employment_status = message[7];
		Real sender_economic_status = message[8];
		Real sender_centrality = message[9];
		Real sender_fake_belief = message[10];
		//message[11] es el sender_id
		Real sender_political_affinity = message[12];

		Real shared_traits_proportion = Real(1) - ((abs( (age - sender_age).value()) + 
										abs( (university_studies - sender_university_studies).value()) + 
										abs((political_involvement - sender_political_involvement).value()) +
										abs((employment_status - employment_status).value()) +
										abs((economic_status - sender_economic_status).value())) / 5); // dividi por 5 porque cada termino puede valer entre 0 y 1

		Real belief =   (sender_fake_belief * 0.125 +
						shared_traits_proportion * 0.125 +
						sender_centrality * 0.25 +
						(Real(1) - abs((political_affinity - sender_political_affinity).value()) ) * 0.5)*0.5;

		return belief; 
}
// put your javascript code here
var categories_template, animal_template;

var curr_category; 

function showTemplate(template, data){
	var html = template(data);
	$('#content').html(html);
}

function displayHome() {
	showTemplate(categories_template, animals_data);
	$(".item").hover(function() {
		var id = $(this).data("id");
		curr_category = animals_data.category[id];
	});
	$(".animal_name").click(function() {
		var id = $(this).data("id");
		var curr_animal = curr_category.animals[id];
		showTemplate(animal_template, curr_animal);
		$(".home").click(function() {
			displayHome();
		});
	});
}

$(document).ready(function(){
	var source = $("#categories-template").html();
	categories_template = Handlebars.compile(source);
	source = $("#animal-template").html();
	animal_template = Handlebars.compile(source);
	displayHome();
});